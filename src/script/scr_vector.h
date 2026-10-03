#pragma once
float *Scr_AllocVector();
const float *Scr_AllocVector(const float *vector);
void AddRefToVector(const float *vector);
void RemoveRefToVector(const float *vector);
