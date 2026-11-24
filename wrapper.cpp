// WORLD 보코더를 브라우저(WebAssembly)에서 쓰기 위한 얇은 래퍼
// JS에서는 _world_f0_length / _world_fft_size / _world_analyze / _world_synthesize 를 호출한다.
#include <emscripten.h>
#include <vector>

#include "world/cheaptrick.h"
#include "world/d4c.h"
#include "world/harvest.h"
#include "world/synthesis.h"

extern "C" {

// 분석 결과 프레임 수
EMSCRIPTEN_KEEPALIVE
int world_f0_length(int x_length, int fs, double frame_period) {
  return GetSamplesForHarvest(fs, x_length, frame_period);
}

// 스펙트럼 FFT 크기 (스펙트럼 한 프레임의 길이는 fft_size / 2 + 1)
EMSCRIPTEN_KEEPALIVE
int world_fft_size(int fs, double f0_floor) {
  CheapTrickOption option;
  InitializeCheapTrickOption(fs, &option);
  option.f0_floor = f0_floor;
  return GetFFTSizeForCheapTrick(fs, &option);
}

// 음성 x 를 분석해서 f0, 스펙트럼(sp), 비주기성(ap)을 채운다.
// sp_flat, ap_flat 은 f0_length * (fft_size / 2 + 1) 크기의 연속 배열.
EMSCRIPTEN_KEEPALIVE
void world_analyze(const double *x, int x_length, int fs, double frame_period,
                   double f0_floor, double f0_ceil, double *f0,
                   double *sp_flat, double *ap_flat) {
  const int f0_length = GetSamplesForHarvest(fs, x_length, frame_period);
  std::vector<double> temporal_positions(f0_length);

  HarvestOption harvest_option;
  InitializeHarvestOption(&harvest_option);
  harvest_option.frame_period = frame_period;
  harvest_option.f0_floor = f0_floor;
  harvest_option.f0_ceil = f0_ceil;
  Harvest(x, x_length, fs, &harvest_option, temporal_positions.data(), f0);

  CheapTrickOption ct_option;
  InitializeCheapTrickOption(fs, &ct_option);
  ct_option.f0_floor = f0_floor;
  ct_option.fft_size = GetFFTSizeForCheapTrick(fs, &ct_option);
  const int fft_size = ct_option.fft_size;
  const int bins = fft_size / 2 + 1;

  std::vector<double *> sp_rows(f0_length), ap_rows(f0_length);
  for (int i = 0; i < f0_length; ++i) {
    sp_rows[i] = sp_flat + (size_t)i * bins;
    ap_rows[i] = ap_flat + (size_t)i * bins;
  }

  CheapTrick(x, x_length, fs, temporal_positions.data(), f0, f0_length,
             &ct_option, sp_rows.data());

  D4COption d4c_option;
  InitializeD4COption(&d4c_option);
  D4C(x, x_length, fs, temporal_positions.data(), f0, f0_length, fft_size,
      &d4c_option, ap_rows.data());
}

// f0, sp, ap 로 다시 음성 y(길이 y_length)를 합성한다.
EMSCRIPTEN_KEEPALIVE
void world_synthesize(const double *f0, int f0_length, const double *sp_flat,
                      const double *ap_flat, int fft_size, double frame_period,
                      int fs, int y_length, double *y) {
  const int bins = fft_size / 2 + 1;
  std::vector<const double *> sp_rows(f0_length), ap_rows(f0_length);
  for (int i = 0; i < f0_length; ++i) {
    sp_rows[i] = sp_flat + (size_t)i * bins;
    ap_rows[i] = ap_flat + (size_t)i * bins;
  }
  Synthesis(f0, f0_length, sp_rows.data(), ap_rows.data(), fft_size,
            frame_period, fs, y_length, y);
}

}  // extern "C"
