#ifndef FRAME_H
#define FRAME_H

#define FRAME_CLOCK 270000

#define FRAME_H_SYNC 62
#define FRAME_H_PORCH_FRONT 16
#define FRAME_H_PORCH_BACK 60
#define FRAME_H_BLANK (FRAME_H_PORCH_FRONT + FRAME_H_SYNC + FRAME_H_PORCH_BACK)

#define FRAME_V_SYNC 6
#define FRAME_V_PORCH_FRONT 9
#define FRAME_V_PORCH_BACK 30
#define FRAME_V_BLANK (FRAME_V_PORCH_FRONT + FRAME_V_SYNC + FRAME_V_PORCH_BACK)

#define FRAME_VIS_WIDTH 720
#define FRAME_VIS_HEIGHT 480

#define FRAME_WIDTH (FRAME_H_BLANK + FRAME_VIS_WIDTH)
#define FRAME_HEIGHT (FRAME_V_BLANK + FRAME_VIS_HEIGHT)

#endif