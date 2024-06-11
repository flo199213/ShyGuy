/***************************************************
Copyright (c) 2020 Luis Llamas
(www.luisllamas.es)

This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at your option) any later version. 

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License along with this program.  If not, see <http://www.gnu.org/licenses 
****************************************************/


#include "Face.h"

Face::Face(Adafruit_ST7789* tft, uint16_t screenWidth, uint16_t screenHeight, uint16_t eyeSize) :
  LeftEye(*this),
  RightEye(*this),
  Blink(*this),
  Look(*this),
  Behavior(*this),
  Expression(*this)
{
  // Set display variable
  _tft = tft;
  
  // Generate canvas
  _canvas = new GFXcanvas16(screenWidth, screenHeight);

  // Set width and height
	Width = screenWidth;
	Height = screenHeight;
	EyeSize = eyeSize;

  // Determine center
	CenterX = Width / 2;
	CenterY = Height / 2;

  // One eye is mirrored
	LeftEye.IsMirrored = true;

  // Initialize behavior
  Behavior.Clear();
	Behavior.Timer.Start();
}

void Face::LookFront()
{
	Look.LookAt(0.0, 0.0);
}

void Face::LookRight()
{
	Look.LookAt(-1.0, 0.0);
}

void Face::LookLeft()
{
	Look.LookAt(1.0, 0.0);
}

void Face::LookTop()
{
	Look.LookAt(0.0, 1.0);
}

void Face::LookBottom()
{
	Look.LookAt(0.0, -1.0);
}

void Face::DoBlink()
{
	Blink.Blink();
}

void Face::Update(uint32_t color, uint32_t backGroundColor, bool draw)
{
  FaceDebug("[FACE] Face: Start Update");

	if (RandomBehavior)
  {
    FaceDebug("[FACE] Face: Behavior.Update");
    Behavior.Update();
  }

	if (RandomLook)
  {
    FaceDebug("[FACE] Face: Look.Update");
    Look.Update();
  }

	if (RandomBlink)
  {
    FaceDebug("[FACE] Face: Blink.Update");
    Blink.Update();
  }
  
  if (draw)
  {
    Draw(color, backGroundColor);
  }
  
  FaceDebug("[FACE] Face: End Update");
}

void Face::Draw(uint32_t color, uint32_t backGroundColor)
{
  FaceDebug("[FACE] Face: Start Draw");

  _canvas->fillScreen(backGroundColor);

  // Draw left eye
  FaceDebug("[FACE] Face: LeftEye.Draw");
	LeftEye.CenterX = CenterX - EyeSize / 2 - EyeInterDistance;
	LeftEye.CenterY = CenterY;
	LeftEye.Draw(_canvas, color, backGroundColor);

  // Draw right eye
  FaceDebug("[FACE] Face: RightEye.Draw");
	RightEye.CenterX = CenterX + EyeSize / 2 + EyeInterDistance;
	RightEye.CenterY = CenterY;
	RightEye.Draw(_canvas, color, backGroundColor);

  _tft->drawRGBBitmap(0, 0, _canvas->getBuffer(), _canvas->width(), _canvas->height());

  FaceDebug("[FACE] Face: End Draw");
}
