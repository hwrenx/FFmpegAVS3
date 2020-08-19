/*
 *  AVS3 related definition
 *
 * Copyright (C) 2020 Huiwen Ren, <hwrenx@gmail.com>
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

#ifndef AVCODEC_AVS3_H
#define AVCODEC_AVS3_H

#define AVS3_NAL_START_CODE          0x010000
#define AVS3_SEQ_START_CODE          0xB0
#define AVS3_SEQ_END_CODE            0xB1
#define AVS3_USER_DATA_START_CODE    0xB2
#define AVS3_INTRA_PIC_START_CODE    0xB3
#define AVS3_UNDEF_START_CODE        0xB4
#define AVS3_EXTENSION_START_CODE    0xB5
#define AVS3_INTER_PIC_START_CODE    0xB6
#define AVS3_VIDEO_EDIT_CODE         0xB7
#define AVS3_FIRST_SLICE_START_CODE  0x00
#define AVS3_PROFILE_BASELINE_MAIN   0x20
#define AVS3_PROFILE_BASELINE_MAIN10 0x22

#define ISPIC(x) ((x) == AVS3_INTRA_PIC_START_CODE || (x) == AVS3_INTER_PIC_START_CODE)
#define ISUNIT(x) ((x) == AVS3_SEQ_START_CODE || ISPIC(x))

#include "libavutil/avutil.h"
#include "libavutil/pixfmt.h"
#include "libavutil/rational.h"

extern const AVRational ff_avs3_frame_rate_tab[16];
extern const int ff_avs3_color_primaries_tab[10];
extern const int ff_avs3_color_transfer_tab[15];
extern const int ff_avs3_color_matrix_tab[12];
extern const enum AVPictureType ff_avs3_image_type[4];

#endif /* AVCODEC_AVS3_H */
