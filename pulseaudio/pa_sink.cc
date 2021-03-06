/* -*- c++ -*- */
/*
 * Copyright 2011 Alexandru Csete OZ9AEC.
 *
 * Gqrx is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * Gqrx is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Gqrx; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */
#include "pa_sink.h"
#include <gr_io_signature.h>
#include <gruel/high_res_timer.h>
#include <stdio.h>
//#include <iostream>

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>



/*! \brief Create a new pulseaudio sink object.
 *  \param device_name The name of the audio device, or NULL for default.
 *  \param audio_rate The sample rate of the audio stream.
 *  \param app_name Application name.
 *  \param stream_name The audio stream name.
 *
 * This is effectively the public constructor for pa_sink.
 */
pa_sink_sptr make_pa_sink(const string device_name, int audio_rate,
                          const string app_name, const string stream_name)
{
    return gnuradio::get_initial_sptr(new pa_sink(device_name, audio_rate, app_name, stream_name));
}


pa_sink::pa_sink(const string device_name, int audio_rate,
                 const string app_name, const string stream_name)
  : gr_sync_block ("pa_sink",
        gr_make_io_signature (1, 1, sizeof(float)),
        gr_make_io_signature (0, 0, 0)),
    d_app_name(app_name),
    d_stream_name(stream_name),
    d_auto_flush(300)
{
    int error;

    /* The sample type to use */
    d_ss.format = PA_SAMPLE_FLOAT32LE;
    d_ss.rate = audio_rate;
    d_ss.channels = 1;

    /* Buffer attributes tuned for low latency, see Documentation/Developer/Clients/LactencyControl */
    size_t latency = pa_usec_to_bytes(10000, &d_ss);
    d_attr.maxlength = d_attr.minreq = d_attr.prebuf = (uint32_t)-1;
    d_attr.fragsize  = latency;
    d_attr.tlength   = latency;

    d_pasink = pa_simple_new(NULL,
                             d_app_name.c_str(),
                             PA_STREAM_PLAYBACK,
                             device_name.empty() ? NULL : device_name.c_str(),
                             d_stream_name.c_str(),
                             &d_ss,
                             NULL,
                             &d_attr,
                             &error);

    if (!d_pasink) {
        /** FIXME: Throw an exception **/
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
    }

}


pa_sink::~pa_sink()
{
    if (d_pasink) {
        pa_simple_free(d_pasink);
    }
}

bool pa_sink::start()
{
    d_last_flush = gruel::high_res_timer_now();

    return true;
}

bool pa_sink::stop()
{
    return true;
}


/*! \brief Select a new pulseaudio output device.
 *  \param device_name The name of the new output.
 */
void pa_sink::select_device(string device_name)
{
    int error;

    pa_simple_free(d_pasink);

    d_pasink = pa_simple_new(NULL,
                             d_app_name.c_str(),
                             PA_STREAM_PLAYBACK,
                             device_name.empty() ? NULL : device_name.c_str(),
                             d_stream_name.c_str(),
                             &d_ss,
                             NULL,
                             NULL,
                             &error);

    if (!d_pasink) {
        /** FIXME: Throw an exception **/
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
    }
}


int pa_sink::work (int noutput_items,
                   gr_vector_const_void_star &input_items,
                   gr_vector_void_star &output_items)
{
    const void *data = (const void *) input_items[0];
    int error;

    if (d_auto_flush > 0)
    {
        gruel::high_res_timer_type tnow = gruel::high_res_timer_now();
        if ((tnow-d_last_flush)/gruel::high_res_timer_tps() > d_auto_flush)
        {
            pa_simple_flush(d_pasink, 0);
            d_last_flush = tnow;

#ifndef QT_NO_DEBUG_OUTPUT
            fprintf(stderr, "Flushing pa_sink\n");
#endif
        }
    }

    if (pa_simple_write(d_pasink, data, noutput_items*sizeof(float), &error) < 0) {
        fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
    }


    return noutput_items;
}

