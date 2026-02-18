Set the config.ini to have desired title sequence, window scale and window height and width. Don't worry about surpassing screen size, it will get recorded correctly anyway (e.g. 3840x2160@2x is perfectly fine on 1080p screen). You can also minimize the window and it should still render correctly.

Launch with:
```
YUV444=1 TITLE_SEQUENCE_NAME=4k-scale2-nvenc OPENRCT2_ENCODER=hevc_nvenc OPENRCT2_ENCODER_PRESET=lossless ./openrct2
```

This will produce `out4k-scale2-nvenc.mkv` file with desired title sequence rendered at
```
src/openrct2/scenes/title/TitleSequenceRender.h:14:constexpr uint8_t FPS = 60;
```

You can also play with various encoding params:
```
YUV444=1 TITLE_SEQUENCE_NAME=4k-scale2-nvenc_p7_hq OPENRCT2_ENCODER=hevc_nvenc OPENRCT2_ENCODER_PRESET=p7 OPENRCT2_ENCODER_STATS=stats OPENRCT2_ENCODER_BITRATE=50000000 OPENRCT2_ENCODER_GOP=120 OPENRCT2_ENCODER_TUNE=hq ./openrct2
```

`OPENRCT2_ENCODER_PASS` and `OPENRCT2_ENCODER_STATS` options are not tested yet and may or may not work - it's likely only used after entire pass is done. Keep in mind RCT:Classic sequence uses a "follow random" command, which might result in varying outputs for each run

At the time of writing the "lossless" `hevc_nvenc` results in 4.4GB file vs 1.8GB when using previous implementation in `libvpx-202602` branch. It's possible VP9 encoder via ffmpeg would produce similar results.

On i7-12800H and geforce MX 550 libvpx implementation rendering os 3840x2160@2x resulted in 4-5 FPS, ffmpeg implementation using `hevc_nvenc` was going at 18 FPS.

`I` is for I-Frame
`B` is for B-Frame
`.` is for P-Frame
