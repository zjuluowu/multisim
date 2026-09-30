#pragma once
// Signature evidence: production telephony_types.h GetParameter calls.
extern "C" int GetParameter(const char* key, const char* defaultValue, char* value, unsigned int length);
extern "C" int SetParameter(const char* key, const char* value);
