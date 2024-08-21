/*
	Copyright (c) 2024 xtreme8000

	This file is part of picoAVE.

	picoAVE is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	picoAVE is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with picoAVE.  If not, see <http://www.gnu.org/licenses/>.
*/

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

#define FRAME_BUFFER_OFFSET 10
#define FRAME_BUFFER_WIDTH (FRAME_VIS_WIDTH + FRAME_BUFFER_OFFSET)

#endif