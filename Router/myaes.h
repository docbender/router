#pragma once

#include "stdafx.h"
#include <string>
#include <vector>

//std::string Encrypt(std::string text, byte key[16], byte iv[16]);
//std::string Decrypt(std::string encryptedText, byte key[16], byte iv[16]);

bool CheckKey();
bool CheckKey(std::string key, std::string name);

std::vector<std::string> Split(std::string source, std::string delimiter);
