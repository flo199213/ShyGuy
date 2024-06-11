#ifndef COMMON_H
#define COMMON_H

//#define FACE_DEBUG

static inline void FaceDebug(String message)
{
#ifdef FACE_DEBUG
  Serial.println(message);
#endif
}

#endif