/******************************************************************************
*                            recordMyDesktop                                  *
*******************************************************************************
*                                                                             *
*            Copyright (C) 2006,2007,2008 John Varouhakis                     *
*                                                                             *
*                                                                             *
*   This program is free software; you can redistribute it and/or modify      *
*   it under the terms of the GNU General Public License as published by      *
*   the Free Software Foundation; either version 2 of the License, or         *
*   (at your option) any later version.                                       *
*                                                                             *
*   This program is distributed in the hope that it will be useful,           *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
*   GNU General Public License for more details.                              *
*                                                                             *
*   You should have received a copy of the GNU General Public License         *
*   along with this program; if not, write to the Free Software               *
*   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA  *
*                                                                             *
*                                                                             *
*                                                                             *
*   For further information contact me at johnvarouhakis@gmail.com            *
******************************************************************************/

#include "config.h"
#include "rmd_encode_image_buffer.h"

#include "rmd_types.h"

#include <errno.h>


void *rmdEncodeImageBuffer(ProgData *pdata) {
	unsigned int	encode_frameno = 0;

	pdata->th_encoding_clean = 0;

	while (pdata->running) {
		EncData	*enc_data = pdata->enc_data;

		pthread_mutex_lock(&pdata->img_buff_ready_mutex);
		while (pdata->running && encode_frameno >= pdata->capture_frameno)
			pthread_cond_wait(&pdata->image_buffer_ready, &pdata->img_buff_ready_mutex);
		encode_frameno = pdata->capture_frameno;
		pthread_mutex_unlock(&pdata->img_buff_ready_mutex);

		pthread_mutex_lock(&pdata->pause_mutex);
		while (pdata->paused)
			pthread_cond_wait(&pdata->pause_cond, &pdata->pause_mutex);
		pthread_mutex_unlock(&pdata->pause_mutex);

		pthread_mutex_lock(&pdata->yuv_mutex);
		if (theora_encode_YUVin(&enc_data->m_th_st, &enc_data->yuv)) {
			fprintf(stderr, "Encoder not ready!\n");
			pthread_mutex_unlock(&pdata->yuv_mutex);
		} else {
			pthread_mutex_unlock(&pdata->yuv_mutex);

			if (theora_encode_packetout(&enc_data->m_th_st, 0, &enc_data->m_ogg_pckt1) == 1) {
				pthread_mutex_lock(&pdata->libogg_mutex);
				ogg_stream_packetin(&enc_data->m_ogg_ts, &enc_data->m_ogg_pckt1);
				pdata->avd += pdata->frametime;
				pthread_mutex_unlock(&pdata->libogg_mutex);
			}
		}
	}

	//last packet
	pthread_mutex_lock(&pdata->theora_lib_mutex);
	pdata->th_encoding_clean = 1;
	pthread_cond_signal(&pdata->theora_lib_clean);
	pthread_mutex_unlock(&pdata->theora_lib_mutex);

	pthread_exit(&errno);
}

//this function is meant to be called normally
//not through a thread of it's own
void rmdSyncEncodeImageBuffer(ProgData *pdata) {
	EncData	*enc_data = pdata->enc_data;

	if (theora_encode_YUVin(&enc_data->m_th_st, &enc_data->yuv)) {
		fprintf(stderr, "Encoder not ready!\n");
		return;
	}

	if (theora_encode_packetout(&enc_data->m_th_st, !pdata->running, &enc_data->m_ogg_pckt1) == 1) {
		pthread_mutex_lock(&pdata->libogg_mutex);
		ogg_stream_packetin(&enc_data->m_ogg_ts, &enc_data->m_ogg_pckt1);

		if (!pdata->running)
			enc_data->m_ogg_ts.e_o_s = 1;

		pdata->avd += pdata->frametime;
		pthread_mutex_unlock(&pdata->libogg_mutex);
	}
}