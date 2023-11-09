
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
};

#include <iostream>
#include <opencv2/opencv.hpp>


int main() {
    av_register_all();

    // Initialize FFmpeg codecs and formats
    avcodec_register_all();

    // Create a new AVFormatContext to represent the output format
    AVFormatContext* outFormatContext = nullptr;
    if (avformat_alloc_output_context2(&outFormatContext, nullptr, "mov", "output.mov") < 0) {
        std::cerr << "Could not create output context" << std::endl;
        return -1;
    }

    // Find the ProRes codec
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_PRORES);
    if (!codec) {
        std::cerr << "ProRes codec not found" << std::endl;
        return -1;
    }

    // Initialize the codec context
    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        std::cerr << "Failed to allocate codec context" << std::endl;
        return -1;
    }

    //int profile = FF_PROFILE_PRORES_4444;
    int profile = FF_PROFILE_PRORES_STANDARD;
    AVPixelFormat pix_fmt = profile >= FF_PROFILE_PRORES_4444 ? AV_PIX_FMT_YUV444P10 : AV_PIX_FMT_YUV422P10;
    int hSampling = pix_fmt == AV_PIX_FMT_YUV444P10 ? 1 : 2;

    // Set codec parameters (e.g., width, height, bitrate, etc.)
    int framerate = 25;
    codecContext->width = 1920;
    codecContext->height = 1080;
    codecContext->color_range = AVCOL_RANGE_MPEG; // limited
    codecContext->color_trc = AVCOL_TRC_BT709; 
    codecContext->color_primaries = AVCOL_PRI_BT709;
    codecContext->colorspace = AVCOL_SPC_BT709;
  
    codecContext->profile = profile;
    codecContext->pix_fmt = pix_fmt;
    codecContext->time_base = AVRational{1, framerate};
    codecContext->framerate = AVRational{framerate, 1};

    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Could not open codec" << std::endl;
        return -1;
    }

    // Create a new AVStream for the video
    AVStream* outStream = avformat_new_stream(outFormatContext, codec);
    if (!outStream) {
        std::cerr << "Failed to create new stream" << std::endl;
        return -1;
    }

    // Copy the codec parameters to the output stream
    avcodec_parameters_from_context(outStream->codecpar, codecContext);
  
    // Open the output file
    if (avio_open(&outFormatContext->pb, "output.mov", AVIO_FLAG_WRITE) < 0) {
        std::cerr << "Could not open output file" << std::endl;
        return -1;
    }
    
    AVDictionary* opts = NULL;
    av_dict_set(&opts, "movflags", "+write_colr", 0);
    
    // Write the file header
    if (avformat_write_header(outFormatContext, &opts) < 0) {
        std::cerr << "Error writing file header" << std::endl;
        return -1;
    }

    // Create a frame for encoding
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        std::cerr << "Could not allocate frame" << std::endl;
        return -1;
    }

    // Load an image from a file
    cv::Mat image = cv::imread("/home/jon/dev/colour/ReferenceHD/colorbars.jpg");

    if (image.empty()) {
        std::cerr << "Could not open or find the image." << std::endl;
        return -1;
    }

    // convert to 10 bit

    cv::Mat image10bit;
    image.convertTo(image10bit, CV_16U, 1023.0 / 255.0);

  
    // Initialize the frame parameters
    frame->format = pix_fmt;
    frame->width = 1920;
    frame->height = 1080;
    frame->color_primaries = AVCOL_PRI_BT709;
    frame->color_trc = AVCOL_TRC_BT709;
    frame->colorspace = AVCOL_SPC_BT709;
    frame->color_range = AVCOL_RANGE_MPEG;
    

    // Allocate memory for the frame data
    if (av_frame_get_buffer(frame, 0) < 0) {
      std::cerr << "Could not allocate frame data" << std::endl;
      return -1;
    }

    // Initialize a packet for holding the encoded data
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = nullptr;
    packet.size = 0;


    // Define the dimensions of your YUV image.
    int width = frame->width;
    int height = frame->height;


    for (int y = 0; y < height; y++) {

      uint16_t* row = (uint16_t*)(frame->data[0] + y * frame->linesize[0]);
      uint16_t* rowU = (uint16_t*)(frame->data[1] + y * frame->linesize[1]);
      uint16_t* rowV = (uint16_t*)(frame->data[2] + y * frame->linesize[2]);
            

      uint16_t* row16bit = image10bit.ptr<uint16_t>(y);

      if ( hSampling == 1 ) {
        for (int x = 0; x < width; x++) {
          uint16_t* pixel1 = row16bit + x * 3;
          uint16_t y1 = (pixel1[0] * 66 + pixel1[1] * 129 + pixel1[2] * 25 + 128) >> 8;
          row[x] = y1;

          float R_norm = static_cast<float>(pixel1[2]) / 1023.0;
          float G_norm = static_cast<float>(pixel1[1]) / 1023.0;
          float B_norm = static_cast<float>(pixel1[0]) / 1023.0;


          // Calculate YUV components
          float Y = 0.299 * R_norm + 0.587 * G_norm + 0.114 * B_norm;
          float U = (B_norm - Y) * 0.493;
          float V = (R_norm - Y) * 0.877;

          // Convert the YUV components back to 10-bit values (if needed)
          uint16_t U_10bit = static_cast<uint16_t>((U + 0.5) * 1023);
          uint16_t V_10bit = static_cast<uint16_t>((V + 0.5) * 1023);

          rowU[x] = U_10bit;
          rowV[x] = V_10bit;
        }
      } else {

        for (int x = 0; x < width; x += 2) {

          // extract RGB
          uint16_t* pixel1 = row16bit + x * 3;
          uint16_t* pixel2 = row16bit + (x + 1) * 3;

          uint16_t y1 = (pixel1[0] * 66 + pixel1[1] * 129 + pixel1[2] * 25 + 128) >> 8;
          uint16_t y2 = (pixel2[0] * 66 + pixel2[1] * 129 + pixel2[2] * 25 + 128) >> 8;

      
          float R_norm = static_cast<float>(pixel1[2]) / 1023.0;
          float G_norm = static_cast<float>(pixel1[1]) / 1023.0;
          float B_norm = static_cast<float>(pixel1[0]) / 1023.0;

          // Calculate YUV components
          float Y = 0.299 * R_norm + 0.587 * G_norm + 0.114 * B_norm;
          float U = (B_norm - Y) * 0.493;
          float V = (R_norm - Y) * 0.877;

          // Convert the YUV components back to 10-bit values (if needed)
          uint16_t U_10bit = static_cast<uint16_t>((U + 0.5) * 1023);
          uint16_t V_10bit = static_cast<uint16_t>((V + 0.5) * 1023);
          row[x] = y1; 
          row[x + 1] = y2;
          
          rowU[x/2] = U_10bit;
          rowV[x/2] = V_10bit;
        }
      }
    }



    // Encode a few frames here (you can replace this with your own frame data)
    for (int i = 0; i < 10; i++) {


      frame->pts = i * 90000/25;

   
      // Encode the frame
      int ret = avcodec_send_frame(codecContext, frame);
      if (ret < 0) {
        std::cerr << "Error sending frame to codec" << std::endl;
        return -1;
      }

      while (ret >= 0) {
        ret = avcodec_receive_packet(codecContext, &packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
          break;
        } else if (ret < 0) {
          std::cerr << "Error encoding frame" << std::endl;
          return -1;
        }

        // Write the encoded data to the output file
        if (av_write_frame(outFormatContext, &packet) < 0) {
          std::cerr << "Error writing frame" << std::endl;
          return -1;
        }

        av_packet_unref(&packet);
      }
    }

    // Clean up and close the output file
    av_write_trailer(outFormatContext);

    av_packet_unref(&packet);
    av_frame_free(&frame);
    avcodec_free_context(&codecContext);
    avio_close(outFormatContext->pb);


    return 0;
}

