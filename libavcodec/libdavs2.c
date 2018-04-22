/*
 * AVS2 decoding using the davs2 library
 * Copyright (C) 2018 Yiqun Xu, <yiqun.xu@vipl.ict.ac.cn>
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

#include "libavutil/avassert.h"
#include "libavutil/common.h"
#include "libavutil/avutil.h"
#include "avcodec.h"
#include "libavutil/imgutils.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <davs2.h>

typedef struct DAVS2Context {
    AVCodecContext *avctx;
    void *dec_handle;
    int got_seqhdr;

    void *decoder;

    AVFrame *frame;
    davs2_param_t    param;      // decoding parameters
    davs2_packet_t   packet;     // input bitstream
    int ret; 

    int img_width[3];
    int img_height[3];
    int out_flag;
    int decoded_frames;
    
    davs2_picture_t  out_frame;  // output data, frame data
    davs2_seq_info_t headerset;  // output data, sequence header

}DAVS2Context;

static int ff_davs2_init(AVCodecContext *avctx)
{
    DAVS2Context *cad = avctx->priv_data;

    /* init the decoder */
    cad->param.threads      = 0;
    cad->param.i_info_level = 0;
    cad->decoder = davs2_decoder_open(&cad->param);
    avctx->flags |= AV_CODEC_FLAG_TRUNCATED;  

    av_log(avctx, AV_LOG_WARNING, "[davs2] decoder created. 0x%llx\n", cad->decoder);
    return 0;
}

/* ---------------------------------------------------------------------------
 */
static int DumpFrames(AVCodecContext *avctx, davs2_picture_t *pic, davs2_seq_info_t *headerset, AVFrame *frm)
{
    DAVS2Context *cad = avctx->priv_data;
    avctx->flags |= AV_CODEC_FLAG_TRUNCATED;

    if (headerset == NULL) {
        return 0;
    }

    if (pic == NULL || pic->ret_type == DAVS2_GOT_HEADER) {
        avctx->frame_size   = (headerset->horizontal_size * headerset->vertical_size * 3 * headerset->bytes_per_sample) >> 1;
        avctx->coded_width  = headerset->horizontal_size;
        avctx->coded_height = headerset->vertical_size;
        avctx->width        = headerset->horizontal_size;
        avctx->height       = headerset->vertical_size;
        avctx->pix_fmt      = (headerset->output_bitdepth == 10 ? AV_PIX_FMT_YUV420P10 : AV_PIX_FMT_YUV420P);
        avctx->framerate.num = (int)(headerset->frame_rate);
        avctx->framerate.den = 1;
        return 0;
    }

    const int bytes_per_sample = pic->bytes_per_sample;
    int i;

    for (i = 0; i < 3; ++i) {
        int size_plane = pic->width[i] * pic->lines[i] * bytes_per_sample;
        frm->buf[i]      = av_buffer_alloc(size_plane);
        frm->data[i]     = frm->buf[i]->data;
        frm->linesize[i] = pic->width[i] * bytes_per_sample;
        memcpy(frm->data[i], pic->planes[i], size_plane);
    }

    frm->width  = cad->headerset.horizontal_size;
    frm->height = cad->headerset.vertical_size;
    frm->pts    = cad->out_frame.pts;

    frm->key_frame = 1;
    frm->pict_type = AV_PICTURE_TYPE_I;
    frm->format = avctx->pix_fmt;
    cad->out_flag = 1;

    cad->decoded_frames++;
    return 1;
}

static int ff_davs2_end(AVCodecContext *avctx)
{
    DAVS2Context *cad = avctx->priv_data;

    /* close the decoder */
    if (cad->decoder != NULL) {
        davs2_decoder_close(cad->decoder);
        av_log(avctx, AV_LOG_WARNING, "[davs2] decoder destroyed. 0x%llx; frames %d\n", cad->decoder, cad->decoded_frames);        
        cad->decoder = NULL;
    }

    return 0;
}

static int davs2_decode_frame(AVCodecContext *avctx, void *data, int *got_frame, AVPacket *avpkt)
{
    DAVS2Context *cad = avctx->priv_data;
    int buf_size       = avpkt->size;
    const uint8_t *buf_ptr = avpkt->data;
    AVFrame *frm = data;

    *got_frame = 0;
    cad->out_flag=0;
    cad->frame = frm;
    avctx->flags |= AV_CODEC_FLAG_TRUNCATED;

    if (buf_size == 0) {
        cad->packet.data = buf_ptr;
        cad->packet.len  = buf_size;
        cad->packet.pts  = avpkt->pts;
        cad->packet.dts  = avpkt->dts;

        for (;;) {
            cad->ret = davs2_decoder_flush(cad->decoder, &cad->headerset, &cad->out_frame);
    
            if (cad->ret < 0) {
                return 0;
            }
    
            if (cad->out_frame.ret_type != DAVS2_DEFAULT) {
                *got_frame = DumpFrames(avctx, &cad->out_frame, &cad->headerset, frm);
                davs2_decoder_frame_unref(cad->decoder, &cad->out_frame);
            }
            if(cad->out_flag == 1) { 
                break;
            }
        }
        return 0;
    } else {
        for(; buf_size > 0;) {
            int len = buf_size;   // for API-3, pass all data in
    
            cad->packet.marker = 0;
            cad->packet.data = buf_ptr;
            cad->packet.len  = len;
            cad->packet.pts  = avpkt->pts;
            cad->packet.dts  = avpkt->dts;

            // 返回使用的码流长度
            len = davs2_decoder_decode(cad->decoder, &cad->packet, &cad->headerset, &cad->out_frame);

            if (cad->out_frame.ret_type != DAVS2_DEFAULT) {
                *got_frame = DumpFrames(avctx, &cad->out_frame, &cad->headerset, frm);
                davs2_decoder_frame_unref(cad->decoder, &cad->out_frame);
            }
    
            if (len < 0) {
                av_log(NULL, AV_LOG_ERROR, "An decoder error counted\n");

                return -1;
            }
    
            buf_ptr     += len;
            buf_size    -= len;

            if(cad->out_flag == 1) {
                break;
            }
        }
    }

    buf_size = (buf_ptr - avpkt->data);

    return buf_size;
}

AVCodec ff_libdavs2_decoder = {
    .name           = "libdavs2",
    .long_name      = NULL_IF_CONFIG_SMALL("Decoder for Chinese AVS2"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_AVS2,
    .priv_data_size = sizeof(DAVS2Context),
    .init           = ff_davs2_init,
    .close          = ff_davs2_end,
    .decode         = davs2_decode_frame,
    .capabilities   =  AV_CODEC_CAP_DELAY,//AV_CODEC_CAP_DR1 |
    .pix_fmts       = (const enum AVPixelFormat[]) { AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV420P10,
                                                     AV_PIX_FMT_NONE },
};
