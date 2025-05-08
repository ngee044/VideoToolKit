video_pipeline_demo

```
#include <VideoToolKit/VideoCapture.h>
#include <iostream>

extern "C" {
#include <libavutil/imgutils.h>   // av_frame_free
}

int main(int argc, char* argv[])
{
    using namespace VideoToolKit;

    // ① 입력 소스 결정: argv[1] 있으면 파일, 없으면 카메라(0)
    std::string src = (argc > 1) ? argv[1] : "0";
    bool useCamera  = (src == "0");

    // ② VideoCapture 객체 생성
    VideoCapture cap;

    // ③ 열기: RGB 변환·GPU 사용 안 함
    if (!cap.open(useCamera ? "0"            // 카메라 장치 ID 0
                            : src,           // 파일 경로
                  /*useGPU*/false,
                  PixelFormat::RGB))         // 출력은 RGB24
    {
        std::cerr << "❌ 입력을 열 수 없습니다.\n";
        return -1;
    }

    // ④ 동기식으로 300프레임 읽기
    int total = 300;
    for (int i = 0; i < total; ++i)
    {
        AVFrame* frame = nullptr;
        if (!cap.grabFrame(&frame))
        {
            std::cout << "스트림 끝 또는 오류, 루프 종료\n";
            break;
        }

        std::cout << "Frame " << i
                  << " | " << frame->width << "x" << frame->height
                  << " | PTS=" << frame->pts
                  << std::endl;

        // ▶ 실제 애플리케이션이라면 여기서
        //   - CUDA/OpenCV 처리
        //   - Texture 업로드
        //   - 인코더로 전달 … 등을 수행
        av_frame_free(&frame);   // 사용 완료 → 해제
    }

    cap.close();
    std::cout << "Capture finished.\n";
    return 0;
}

cap.startCaptureAsync([](AVFrame* f){          // 콜백 버전
    static std::atomic<int> cnt{0};
    if(++cnt % 60 == 0) std::cout << "decoded 60 frames\n";
    av_frame_free(&f);                         // 사용 후 해제 필요
});
std::this_thread::sleep_for(std::chrono::seconds(10));
cap.stopCapture();
```
