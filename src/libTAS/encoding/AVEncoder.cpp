/*
    Copyright 2015-2018 Clément Gallet <clement.gallet@ens-lyon.org>

    This file is part of libTAS.

    libTAS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libTAS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libTAS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "AVEncoder.h"

#include "../logging.h"
#include "../ScreenCapture.h"
#include "../audio/AudioContext.h"
#include "../global.h" // shared_config
#include "../GlobalState.h"

#include <cstdint>
#include <unistd.h> // usleep

namespace libtas {

AVEncoder::AVEncoder(SDL_Window* window) {
    std::string commandline = "ffmpeg -hide_banner -y -f nut -i - ";
    commandline += ffmpeg_options;
    commandline += " \"";
    commandline += dumpfile;
    commandline += "\"";

    NATIVECALL(ffmpeg_pipe = popen(commandline.c_str(), "w"));

    if (! ffmpeg_pipe) {
        debuglog(LCF_DUMP | LCF_ERROR, "Could not create a pipe to ffmpeg");
        return;
    }

    int width, height;
    ScreenCapture::getDimensions(width, height);

    const char* pixfmt = ScreenCapture::getPixelFormat();

    nutMuxer = new NutMuxer(width, height, shared_config.framerate_num, shared_config.framerate_den, pixfmt, audiocontext.outFrequency, audiocontext.outAlignSize, audiocontext.outNbChannels, ffmpeg_pipe);
}

void AVEncoder::encodeOneFrame(bool draw) {
    if (!nutMuxer)
        return;

    /*** Audio ***/
    debuglog(LCF_DUMP, "Encode an audio frame");

    nutMuxer->writeAudioFrame(audiocontext.outSamples.data(), audiocontext.outBytes);

    /*** Video ***/
    debuglog(LCF_DUMP, "Encode a video frame");

    int size;
    /* Access to the screen pixels if the current frame is a draw frame
     * or if we never drew. If not, we will capture the last drawn frame.
     */
    if (draw || !pixels) {
        size = ScreenCapture::getPixels(&pixels);
    }

    nutMuxer->writeVideoFrame(pixels, size);
}

AVEncoder::~AVEncoder() {
    if (nutMuxer) {
        nutMuxer->finish();
    }

    if (ffmpeg_pipe) {
        int ret;
        NATIVECALL(ret = pclose(ffmpeg_pipe));
        if (ret < 0) {
            debuglog(LCF_DUMP | LCF_ERROR, "Could not close the pipe to ffmpeg");
        }
    }
}

char AVEncoder::dumpfile[4096] = {0};
char AVEncoder::ffmpeg_options[4096] = {0};

std::unique_ptr<AVEncoder> avencoder;

}

// #endif