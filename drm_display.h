//
//  drm_display.h
//
//  Created by Andries Salm on 07-08-21.
//  Copyright (c) 2021 Andries Salm. All rights reserved.
//

#ifndef Drm_Display_h
#define Drm_Display_h

#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/hwcontext_drm.h"
#include <libavdevice/avdevice.h>

#include "pthread.h"
#include <semaphore.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

static void * displayThread(void *);

class DrmFrame {
public: 
    int layer = 0;
    int data = 0;
    unsigned long long time;
    bool flush;
};

class DrmDisplay {
public:

    void init(); //TODO
    void deinit(); //TODO

    int _drmFileDescriptor = -1;
    uint32_t _connectorId = 0;
    uint32_t _crtcId;
    int _crtcIdx;
    bool _requestTerminate = false;

    //Composition of CRTC
    int _crtcX;
    int _crtcY;
    int _crtcWidth;
    int _crtcHeight;

    pthread_t _displayThread;

    int _findCrtc(); 
    int _findPlane(); //TODO
    int _displayLayer(); //TODO

    // static int find_plane( const int drmfd, const int crtcidx, const uint32_t format, uint32_t * const pplane_id)
    // static int do_display(drm_display_env_t * const de, AVFrame * frame)

    // static int drm_vout_write_packet(drm_display_env_t * de, AVFrame *src_frame)
    // static int do_sem_wait(sem_t * const sem, const int nowait)
    // static void da_uninit(drm_display_env_t * const de, drm_aux_t * da)
    

};

#endif
