/*
 * Copyright (c) 2020 John Cox for Raspberry Pi Trading
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


// *** This module is a work in progress and its utility is strictly
//     limited to testing.

#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/hwcontext_drm.h"
//#include "libavformat/internal.h"
enum AVWriteUncodedFrameFlags {
    AV_WRITE_UNCODED_FRAME_QUERY           = 0x0001,
};
#include <libavdevice/avdevice.h>

#include "pthread.h"
#include <semaphore.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <time.h> 
clock_t current_ticks, delta_ticks;
double fps = 0;


#define TRACE_ALL 0

#define DRM_MODULE "vc4"

#define ERRSTR strerror(errno)

struct drm_setup {
   int conId;
   uint32_t crtcId;
   int crtcIdx;
   uint32_t planeId;
   unsigned int out_fourcc;
   struct {
       int x, y, width, height;
   } compose;
};

typedef struct drm_aux_s {
    unsigned int fb_handle;
    AVFrame * frame;
} drm_aux_t;

// Aux size should only need to be 2, but on a few streams (Hobbit) under FKMS
// we get initial flicker probably due to dodgy drm timing
#define AUX_SIZE 3
typedef struct drm_display_env_s
{
    AVClass *class;

    int drm_fd;
    uint32_t con_id;
    struct drm_setup setup;
    enum AVPixelFormat avfmt;
    int show_all;

    unsigned int ano;
    drm_aux_t aux[AUX_SIZE];

    pthread_t q_thread;
    sem_t q_sem_in;
    sem_t q_sem_out;
    int q_terminate;
    AVFrame * q_next;

} drm_display_env_t;


static int find_plane(
                      const int drmfd, const int crtcidx, const uint32_t format,
                      uint32_t * const pplane_id)
{
   drmModePlaneResPtr planes;
   drmModePlanePtr plane;
   unsigned int i;
   unsigned int j;
   int ret = 0;

   planes = drmModeGetPlaneResources(drmfd);
   if (!planes)
   {
       printf( "drmModeGetPlaneResources failed: %s\n", ERRSTR);
       return -1;
   }

   for (i = 0; i < planes->count_planes; ++i) {
      plane = drmModeGetPlane(drmfd, planes->planes[i]);
      if (!planes)
      {
          printf("drmModeGetPlane failed: %s\n", ERRSTR);
          break;
      }

      if (!(plane->possible_crtcs & (1 << crtcidx))) {
         drmModeFreePlane(plane);
         continue;
      }

      for (j = 0; j < plane->count_formats; ++j) {
         if (plane->formats[j] == format)
            break;
      }

      if (j == plane->count_formats) {
         drmModeFreePlane(plane);
         continue;
      }

      *pplane_id = plane->plane_id;
      drmModeFreePlane(plane);
      break;
   }

   if (i == planes->count_planes)
      ret = -1;

   drmModeFreePlaneResources(planes);
   return ret;
}

static void da_uninit(drm_display_env_t * const de, drm_aux_t * da)
{
    if (da->fb_handle != 0) {
        drmModeRmFB(de->drm_fd, da->fb_handle);
        da->fb_handle = 0;
    }

    av_frame_free(&da->frame);
}

static int do_display(drm_display_env_t * const de, AVFrame * frame)
{
    const AVDRMFrameDescriptor *desc = (AVDRMFrameDescriptor*)frame->data[0];
    drm_aux_t * da = de->aux + de->ano;
    const uint32_t format = desc->layers[0].format;
    int ret = 0;

    if (de->setup.out_fourcc != format) {
        if (find_plane( de->drm_fd, de->setup.crtcIdx, format, &de->setup.planeId)) {
            av_frame_free(&frame);
            printf("No plane for format: %#x\n", format);
            return -1;
        }
        de->setup.out_fourcc = format;
    }

    {
        drmVBlank vbl = {
            .request = {
                .type = DRM_VBLANK_RELATIVE,
                .sequence = 0
            }
        };

        while (drmWaitVBlank(de->drm_fd, &vbl)) {
            if (errno != EINTR) {
                // This error is known, but not code breaking.
                // printf("drmWaitVBlank failed: %s\n", ERRSTR);
                break;
            }
        }
    }

    da_uninit(de, da);

    {
        uint32_t pitches[4] = {0};
        uint32_t offsets[4] = {0};
        uint64_t modifiers[4] = {0};
        uint32_t bo_object_handles[4] = {0};
        uint32_t bo_handles[4] = {0};
        int i, j, n;

        da->frame = frame;

        for (i = 0; i < desc->nb_objects; ++i) {
            if (drmPrimeFDToHandle(de->drm_fd, desc->objects[i].fd, bo_object_handles + i) != 0) {
                printf( "drmPrimeFDToHandle[%d](%d) failed: %s\n", i, desc->objects[i].fd, ERRSTR);
                return -1;
            }
        }

        n = 0;
        for (i = 0; i < desc->nb_layers; ++i) {
            for (j = 0; j < desc->layers[i].nb_planes; ++j) {
                const AVDRMPlaneDescriptor * const p = desc->layers[i].planes + j;
                const AVDRMObjectDescriptor * const obj = desc->objects + p->object_index;
                pitches[n] = p->pitch;
                offsets[n] = p->offset;
                modifiers[n] = obj->format_modifier;
                bo_handles[n] = bo_object_handles[p->object_index];
                ++n;
            }
        }

        if (drmModeAddFB2WithModifiers(de->drm_fd,
                                         av_frame_cropped_width(frame),
                                         av_frame_cropped_height(frame),
                                         desc->layers[0].format, bo_handles,
                                         pitches, offsets, modifiers,
                                         &da->fb_handle, DRM_MODE_FB_MODIFIERS /** 0 if no mods */) != 0) {
            printf("drmModeAddFB2WithModifiers failed: %s\n", ERRSTR);
            return -1;
        }
    }

    ret = drmModeSetPlane(de->drm_fd, de->setup.planeId, de->setup.crtcId,
                              da->fb_handle, 0,
                de->setup.compose.x, de->setup.compose.y,
                de->setup.compose.width,
                de->setup.compose.height,
                0, 0,
                av_frame_cropped_width(frame) << 16,
                av_frame_cropped_height(frame) << 16);

    if (ret != 0) {
        printf( "drmModeSetPlane failed: %s\n", ERRSTR);
    }

    de->ano = de->ano + 1 >= AUX_SIZE ? 0 : de->ano + 1;

    
    //Calculate FPS
    delta_ticks = clock() - current_ticks; //the time, in ms, that took to render the scene
    
    if(delta_ticks > 0) {
        fps = ((double) CLOCKS_PER_SEC) / ((double)delta_ticks);
    }

    printf("%.6f\n", fps);
    

    current_ticks = clock();

    return ret;
}

static int do_sem_wait(sem_t * const sem, const int nowait)
{
    while (nowait ? sem_trywait(sem) : sem_wait(sem)) {
        if (errno != EINTR)
            return -errno;
    }
    return 0;
}

static void * display_thread(void * v)
{
    drm_display_env_t * const de = v;
    int i;

    sem_post(&de->q_sem_out);

    for (;;) {
        AVFrame * frame;

        do_sem_wait(&de->q_sem_in, 0);

        if (de->q_terminate)
            break;

        frame = de->q_next;
        de->q_next = NULL;
        sem_post(&de->q_sem_out);

        do_display( de, frame);
    }

    for (i = 0; i != AUX_SIZE; ++i)
        da_uninit(de, de->aux + i);

    av_frame_free(&de->q_next);

    return NULL;
}

//static int drm_vout_write_packet(AVFormatContext *s, AVPacket *pkt)
static int drm_vout_write_packet(drm_display_env_t * de, AVFrame *src_frame)
{
    //const AVFrame * const src_frame = (AVFrame *)pkt->data;
    AVFrame * frame;
    //drm_display_env_t * const de = s->priv_data;
    int ret;

    if ((src_frame->flags & AV_FRAME_FLAG_CORRUPT) != 0) {
        printf( "Discard corrupt frame: fmt=%d, ts=%" PRId64 "\n", src_frame->format, src_frame->pts);
        return 0;
    }

    if (src_frame->format == AV_PIX_FMT_DRM_PRIME) {
        frame = av_frame_alloc();
        av_frame_ref(frame, src_frame);
    }
    else if (src_frame->format == AV_PIX_FMT_VAAPI) {
        frame = av_frame_alloc();
        frame->format = AV_PIX_FMT_DRM_PRIME;
        if (av_hwframe_map(frame, src_frame, 0) != 0)
        {
            printf( "Failed to map frame (format=%d) to DRM_PRiME\n", src_frame->format);
            av_frame_free(&frame);
            return AVERROR(EINVAL);
        }
    }
    else {
        printf( "Frame (format=%d) not DRM_PRiME\n", src_frame->format);
        return AVERROR(EINVAL);
    }

    ret = do_sem_wait(&de->q_sem_out, !de->show_all);
    if (ret) {
        av_frame_free(&frame);
    }
    else {
        de->q_next = frame;
        sem_post(&de->q_sem_in);
    }

    return 0;
}

static int find_crtc(int drmfd, struct drm_setup *s, uint32_t * const pConId)
{
   int ret = -1;
   int i;
   drmModeRes *res = drmModeGetResources(drmfd);
   drmModeConnector *c;

   if(!res)
   {
      printf( "drmModeGetResources failed: %s\n", ERRSTR);
      return -1;
   }

   if (res->count_crtcs <= 0)
   {
      printf( "drm: no crts\n");
      goto fail_res;
   }

   if (!s->conId) {
      fprintf(stderr,
         "No connector ID specified.  Choosing default from list:\n");

      for (i = 0; i < res->count_connectors; i++) {
         drmModeConnector *con =
            drmModeGetConnector(drmfd, res->connectors[i]);
         drmModeEncoder *enc = NULL;
         drmModeCrtc *crtc = NULL;

         if (con->encoder_id) {
            enc = drmModeGetEncoder(drmfd, con->encoder_id);
            if (enc->crtc_id) {
               crtc = drmModeGetCrtc(drmfd, enc->crtc_id);
            }
         }

         if (!s->conId && crtc) {
            s->conId = con->connector_id;
            s->crtcId = crtc->crtc_id;
         }

      }

      if (!s->conId) {
         printf("No suitable enabled connector found.\n");
         return -1;;
      }
   }

   s->crtcIdx = -1;

   for (i = 0; i < res->count_crtcs; ++i) {
      if (s->crtcId == res->crtcs[i]) {
         s->crtcIdx = i;
         break;
      }
   }

   if (s->crtcIdx == -1)
   {
       printf( "drm: CRTC %u not found\n", s->crtcId);
       goto fail_res;
   }

   if (res->count_connectors <= 0)
   {
       printf( "drm: no connectors\n");
       goto fail_res;
   }

   c = drmModeGetConnector(drmfd, s->conId);
   if (!c)
   {
       printf( "drmModeGetConnector failed: %s\n", ERRSTR);
       goto fail_res;
   }

   if (!c->count_modes)
   {
       printf( "connector supports no mode\n");
       goto fail_conn;
   }

   {
      drmModeCrtc *crtc = drmModeGetCrtc(drmfd, s->crtcId);
      s->compose.x = crtc->x;
      s->compose.y = crtc->y;
      s->compose.width = crtc->width;
      s->compose.height = crtc->height;
      drmModeFreeCrtc(crtc);
   }

   if (pConId)
      *pConId = c->connector_id;
   ret = 0;

fail_conn:
   drmModeFreeConnector(c);

fail_res:
   drmModeFreeResources(res);

   return ret;
}

// deinit is called if init fails so no need to clean up explicity here
//static int drm_vout_init(struct AVFormatContext * s)
static int drm_vout_init(drm_display_env_t * const de)
{
    current_ticks = clock();


    int rv;
    const char * drm_module = DRM_MODULE;

    printf("<<< %s\n", __func__);

    de->drm_fd = -1;
    de->con_id = 0;
    de->setup = (struct drm_setup){0};
    de->q_terminate = 0;

    if ((de->drm_fd = drmOpen(drm_module, NULL)) < 0)
    {
        rv = AVERROR(errno);
        printf("Failed to drmOpen %s: %s\n", drm_module, av_err2str(rv));
        return rv;
    }

    if (find_crtc(de->drm_fd, &de->setup, &de->con_id) != 0)
    {
        printf("failed to find valid mode\n");
        rv = AVERROR(EINVAL);
        goto fail_close;
    }

    sem_init(&de->q_sem_in, 0, 0);
    sem_init(&de->q_sem_out, 0, 0);
    if (pthread_create(&de->q_thread, NULL, display_thread, de)) {
        rv = AVERROR(errno);
        printf("Failed to creatye display thread: %s\n", av_err2str(rv));
        goto fail_close;
    }

    printf(">>> %s\n", __func__);

    return 0;

fail_close:
    close(de->drm_fd);
    de->drm_fd = -1;
    printf(">>> %s: FAIL\n", __func__);

    return rv;
}

//static void drm_vout_deinit(struct AVFormatContext * s)
static void drm_vout_deinit(drm_display_env_t * const de)
{
    //drm_display_env_t * const de = s->priv_data;


    de->q_terminate = 1;
    sem_post(&de->q_sem_in);
    pthread_join(de->q_thread, NULL);
    sem_destroy(&de->q_sem_in);
    sem_destroy(&de->q_sem_out);

    av_frame_free(&de->q_next);

    if (de->drm_fd >= 0) {
        close(de->drm_fd);
        de->drm_fd = -1;
    }

}

