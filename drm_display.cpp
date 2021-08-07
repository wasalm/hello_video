//
//  Service.cpp
//  Websockets
//
//  Created by Andries Salm on 02-07-15.
//  Copyright (c) 2015 Andries Salm. All rights reserved.
//

#include "drm_display.h"

static void displayThread(void * arg1) {
    DrmDisplay * drmDisplay = (DrmDisplay *) arg1;

    //TODO
}

void DrmDisplay::init() {
    this->_drmFileDescriptor = -1;
    this->_connectorId = 0;
    this->_requestTerminate = false;

    // Open DRM device
    if((this->_drmFileDescriptor = drmOpen("vc4", NULL)) < 0) {
        printf("Failed to drmOpen vc4.\n");
    }

    //Find CRTC
    if (this->find_crtc() != 0) {
        printf("failed to find valid mode\n");
        goto fail_close;
    }

    // Start display thread
    if (pthread_create(&(this->_displayThread), NULL, displayThread, this)) {
        printf("Failed to creatye display thread.\n");
        goto fail_close;
    }

    return;

fail_close:
    close(this->_drmFileDescriptor);
    this->_drmFileDescriptor = -1;

    return;
}

void DrmDisplay::deinit() {
    this->_requestTerminate = true;
    pthread_join(this->_displayThread, NULL);

    if(this->_drmFileDescriptor >= 0) {
        close(this->_drmFileDescriptor);
        this->_drmFileDescriptor = -1;
    }
}

void DrmDisplay::_findCrtc() {
    int ret = -1;
    int i;

    drmModeRes *res = drmModeGetResources(this->_drmFileDescriptor);
    drmModeConnector *c;

    if(!res) {
        printf( "drmModeGetResources failed: %s\n", ERRSTR);
        return -1;
    }

    if (res->count_crtcs <= 0) {
        printf( "drm: no crts\n");
        goto fail_res;
    }

    if (!this->_connectorId) {
        fprintf(stderr, "No connector ID specified.  Choosing default from list:\n");

        for (i = 0; i < res->count_connectors; i++) {
            drmModeConnector *con = drmModeGetConnector(drmfd, res->connectors[i]);
            drmModeEncoder *enc = NULL;
            drmModeCrtc *crtc = NULL;

            if (con->encoder_id) {
                enc = drmModeGetEncoder(drmfd, con->encoder_id);
                if (enc->crtc_id) {
                    crtc = drmModeGetCrtc(drmfd, enc->crtc_id);
                }
            }

            if (!this->_connectorId && crtc) {
                this->_connectorId = con->connector_id;
                this->_crtcId = crtc->crtc_id;
            }
        }

        if (!this->_connectorId) {
            printf("No suitable enabled connector found.\n");
            return -1;
        }
    }

    this->_crtcIdx = -1;

    for (i = 0; i < res->count_crtcs; ++i) {
        if (this->_crtcId == res->crtcs[i]) {
            this->_crtcIdx = i;
            break;
        }
    }

    if (this->_crtcIdx == -1) {
        printf( "drm: CRTC %u not found\n", s->crtcId);
        goto fail_res;
    }

    if (res->count_connectors <= 0) {
        printf( "drm: no connectors\n");
        goto fail_res;
    }

    c = drmModeGetConnector(this->_drmFileDescriptor, this->_connectorId);
    if (!c) {
        printf( "drmModeGetConnector failed: %s\n", ERRSTR);
        goto fail_res;
    }

    if (!c->count_modes) {
        printf( "connector supports no mode\n");
        goto fail_conn;
    }

    drmModeCrtc *crtc = drmModeGetCrtc(this->_drmFileDescriptor, this->_crtcId);
    this->_crtcX = crtc->x;
    this->_crtcY = crtc->y;
    this->_crtcWidth = crtc->width;
    this->_crtcHeight = crtc->height;
    drmModeFreeCrtc(crtc);

    ret = 0;

fail_conn:
    drmModeFreeConnector(c);

fail_res:
    drmModeFreeResources(res);

    return ret;
}