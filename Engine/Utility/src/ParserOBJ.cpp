#pragma once
#define valid_digit(c) ((c) >= '0' && (c) <= '9')

#include "ParserOBJ.h"

#include <fstream>
#include <map>

//depending on what results in being the significant bottleneck for this loader, can consider implementing a faster hash function (rather than converting every letter to the hex, but find a combination that works for all the tags)

using namespace std;

namespace Parser {
	namespace OBJ {
		//.obj
		const _int64 _mtllib = 0x6D746C6C6962;
		const _int64 _v = 0x76;
		const _int64 _vt = 0x7674;
		const _int64 _vn = 0x766e;
		const _int64 _usemtl = 0x7573656d746c;
		const _int64 _f = 0x66;

		//mtl
		const _int64 _newmtl = 0x6E65776D746C;
		const _int64 _Ka = 0x4B61;
		const _int64 _Kd = 0x4B64;
		const _int64 _Ks = 0x4B73;
		const _int64 _Ns = 0x4E73;
		const _int64 _d = 0x64;
		const _int64 _Tf = 0x5466;
		const _int64 _Ni = 0x4E69;

		//mtl texture maps
		const _int64 _map_Ka = 0x6D61705F6B64;
		const _int64 _map_Kd = 0x6D61705F4B64;
		const _int64 _map_Ks = 0x6D61705F4B73;
		const _int64 _map_Ns = 0x6D61705F4E73;
		const _int64 _map_d = 0x6D61705F64;
		const _int64 _map_Bump = 0x6D61705F42756D70;
		const _int64 _bump = 0x62756D70;
		const _int64 _disp = 0x64697370;
		const _int64 _decal = 0x646563616C;
		const _int64 _refl = 0x7265666C;

		//mtl texture options



		//these members are not visible to other programs including the header
		size_t GetTag(const char* line, _int64& value);
		bool EvaluateLine(wstring& basePath, string_view& lineView, vector<IND>& ind, vector<VERTEX>& v, vector<POINT>& vn, vector<TEX>& vt, vector<MATERIAL>& mats, map<wstring, int> &dictionary, int &currentMat, int& currentStart);

		IND SplitFace(const char*& in);

		void ReadV2(const string_view& line, const size_t& tag, TEX& vec);
		void ReadV3(const string_view& line, const size_t& tag, POINT& vec);

		//may consider moving this to a fast math class/lib implementation in the future
		float atof(const char*& p);
		int atou(const char*& p);

		bool LoadMTL(wstring& path, vector<MATERIAL>& mats);
		bool EvaluateMTL(wstring& basePath, string_view& lineView, vector<MATERIAL>& mats);
	
		//function definitions
		bool Load(wstring &fPath, vector<IND>& ind, vector<VERTEX>& v, vector<POINT>& vn, vector<TEX>& vt, vector<MATERIAL>& mats) {
			ifstream fileA(fPath, ios::binary); //loading in as binary fixes any encoding issues (that may cause problems with tellg())
			if (!fileA.is_open()) return false;

			//get file path base
			wstring p = fPath.substr(0, fPath.find_last_of(L"/\\") + 1);

			char chunk[8193]; //+1 for the null char
			int toRead = 8192;
			const char* index;

			fileA.seekg(0, fileA.end);
			int length = fileA.tellg();
			fileA.seekg(0, fileA.beg);
			if (length < toRead) //ensure that it can still read in a whole file if it is too small for a single chunk
			{
				toRead = length; 
			} 
			chunk[toRead] = '\0';//ensure the end of buffer is always marked with null char, so nothing reads into protected / invalid memory

			int total = 0; //UINT so that if total ever becomes -1, then it will wrap around to the largest possible number

			int start;
			int end;

			map<wstring, int> dictionary; //for keeping track of materials
			int currentMat = 0; int currentStart = 0;

			string_view line;
			string extra;
			while (total < length)
			{
				fileA.read(chunk, toRead);
				int test = fileA.tellg(); //becomes -1 here as well (fails) - indicating that toRead is too large in this instance
				start = 0;
				end = 0;

				index = chunk;
				while (*index != '\0') { //read to end of buffer
					if (*index == '\n') {
						line = string_view(&chunk[start], end - start);
						start = end + 1; //this statement ensures that start will be equal to toRead if the lines are fully read

						EvaluateLine(p, line, ind, v, vn, vt, mats, dictionary, currentMat, currentStart); //without evaluating lines, takes ~50ms (effectively entirely io); takes ~130ms with evaluating lines
					}

					end++;
					index++;
				}

				if (start != toRead) { //incomplete line; finish line
					getline(fileA, extra);

					extra = string(&chunk[start], toRead - start) + extra;
					string_view line = string_view(extra);

					EvaluateLine(p, line, ind, v, vn, vt, mats, dictionary, currentMat, currentStart);
				}

				total = fileA.tellg();
				if (length - total < toRead) { //ensure that if the file ends partially in the buffer, the program doesn't read past that into previous data (and ensures that all data is correctly read)
					toRead = length - total; chunk[toRead] = '\0';
				}
			}

			fileA.close();
			return true;
		}

		//cannot use string_view.data() to view part of the string - have to create a new one first
		bool EvaluateLine(wstring& basePath, string_view& lineView, vector<IND>& ind, vector<VERTEX>& v, vector<POINT>& vn, vector<TEX>& vt, vector<MATERIAL>& mats, map<wstring, int>& dictionary, int& currentMat, int& currentStart) {
			_int64 iTag = 0;
			size_t tagIndex = 0;
			tagIndex = GetTag(lineView.data(), iTag);
			
			switch (iTag) {
			case _mtllib: //only support for one mtl file
			{
				string_view relative = lineView.substr(tagIndex + 1, lineView.length() - (tagIndex + 1));
				relative = relative.substr(0, relative.find_first_of(' ')); //remove any white spaces at the end of the line
				wstring fullPath(relative.begin(), relative.end());
				fullPath = basePath + fullPath;

				LoadMTL(fullPath, mats); //load in mtl data

				//set up dictionary
				for (int i = 0; i < mats.size(); i++) {
					dictionary.insert({ mats[i].name, i });
				}
				break;
			}

			case _v:
			{
				VERTEX vec(0, 0, 0);
				ReadV3(lineView, tagIndex, vec);
				v.push_back(vec);
				break;
			}

			case _vt:
			{
				TEX vecT(0, 0);
				ReadV2(lineView, tagIndex, vecT);
				vt.push_back(vecT);
				break;
			}

			case _vn:
			{
				POINT vecN(0, 0, 0);
				ReadV3(lineView, tagIndex, vecN);
				vn.push_back(vecN);
				break;
			}

			case _usemtl: //need to adapt to work for duplicate materials
			{
				string_view name = lineView.substr(tagIndex + 1, lineView.length() - (tagIndex + 1));
				name = name.substr(0, name.find_first_of(' ')); //remove any white spaces at the end of the line (-1 works to return whole string still)

				wstring matName(name.begin(), name.end()); //copy string containing name
				mats[dictionary[matName]].start.push_back(ind.size()); //will start the one after the current index (which is ind.size()); length will be calculated later
				currentMat = dictionary[matName];
				currentStart = mats[currentMat].start.size() - 1;
				mats[currentMat].length.push_back(0); //add length value too
				break;
			}

			case _f:
			{
				const char* index = lineView.data() + tagIndex + 1;
				IND orig = SplitFace(index);
				index++;
				IND prev = SplitFace(index);
				index++;

				do {
					IND current = SplitFace(index);
					if (*index == ' ') index++; //avoids ++ if index refers to a null character (at end of file or buffer) where it could read out of bounds

					ind.push_back(orig);
					ind.push_back(prev);
					ind.push_back(current);

					//add to the current material's current length's value
					mats[currentMat].length[currentStart]+= 3; //length in terms of each index, not per triangle
					
					prev = current;
				} while (valid_digit(*index));
				break;
			}

			default: //any unsupported tags (as well as comments and empty lines) end up here
				break;
			}

			return true;
		}

		//could look at templating the code or something similar, since it is identical to Load() except for the line evaluation function it calls (maybe it takes an address of the desired function or something)
		bool LoadMTL(wstring& path, vector<MATERIAL>& mats) {
			ifstream fileA(path, ios::in);
			if (!fileA.is_open()) return false;

			//get file path base
			wstring p(path);
			p = p.substr(0, p.find_last_of(L"/\\") + 1);

			char chunk[8192];
			int toRead = 8192;
			const char* index;

			fileA.seekg(0, fileA.end);
			int length = fileA.tellg();
			fileA.seekg(0, fileA.beg);
			if (length < toRead) //ensure that it can still read in a whole file if it is too small for a single chunk
			{
				toRead = length; chunk[toRead] = '\0';
			}

			int total = 0;

			int start;
			int end;

			string_view line;
			string extra;
			while (total != length)
			{
				fileA.read(chunk, toRead);
				start = 0;
				end = 0;

				index = chunk;
				while (*index != '\0') { //read to end of buffer
					if (*index == '\n') {
						line = string_view(&chunk[start], end - start);
						start = end + 1; //this statement ensures that start will be equal to toRead if the lines are fully read

						EvaluateMTL(p, line, mats);
					}

					end++;
					index++;
				}

				if (start != toRead) { //incomplete line; finish line
					getline(fileA, extra);

					extra = string(&chunk[start], toRead - start) + extra;
					string_view line = string_view(extra);

					EvaluateMTL(p, line, mats);
				}

				total = fileA.tellg();
				if (length - total < toRead) { //ensure that if the file ends partially in the buffer, the program doesn't read past that into previous data (and ensures that all data is correctly read)
					toRead = length - total; chunk[toRead] = '\0';
				}
			}

			fileA.close();
			return true;
		}

		bool EvaluateMTL(wstring& basePath, string_view& lineView, vector<MATERIAL>& mats) {
			_int64 iTag = 0;
			size_t tagIndex = 0;
			tagIndex = GetTag(lineView.data(), iTag);

			//these will always reference the last created material (from _newmtl)
			switch (iTag) {
			case _newmtl:
			{
				string_view name = lineView.substr(tagIndex + 1, lineView.length() - (tagIndex + 1));
				name = name.substr(0, name.find_first_of(' ')); //remove any white spaces at the end of the line (-1 works to return whole string still)

				MATERIAL mat;
				mat.name = wstring(name.begin(), name.end()); //copy string containing name
				mats.push_back(mat);
				break;
			}
			case _Ka:
			{
				POINT ambient(0, 0, 0);
				ReadV3(lineView, tagIndex, ambient);
				mats[mats.size() - 1].colors[INDEX_Ka] = ambient;
				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_AMBIENT;
				break;
			}
			case _Kd:
			{
				POINT diffuse(0, 0, 0);
				ReadV3(lineView, tagIndex, diffuse);
				mats[mats.size() - 1].colors[INDEX_Kd] = diffuse;
				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_DIFFUSE;
				break;
			}
			case _Ks:
			{
				POINT specColor(0, 0, 0);
				ReadV3(lineView, tagIndex, specColor);
				mats[mats.size() - 1].colors[INDEX_Ks] = specColor;
				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_SPECULAR;
				break;
			}
			case _Ns:
			{
				POINT specHighlights(0, 0, 0);
				ReadV3(lineView, tagIndex, specHighlights);
				mats[mats.size() - 1].colors[INDEX_Ns] = specHighlights;
				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_SPECULAR;
				break;
			}
			case _d:
			{
				const char* index = lineView.data() + tagIndex + 1;
				float dissolve = atof(index);
				mats[mats.size() - 1].constants[INDEX_d] = dissolve;
				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_DISSOLVE;
				break;
			}
			case _Tf:
			{
				POINT tFilter(0, 0, 0);
				ReadV3(lineView, tagIndex, tFilter);
				mats[mats.size() - 1].colors[INDEX_Tf] = tFilter;
				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_TR_FILTER;
				break;
			}
			case _Ni:
			{
				const char* index = lineView.data() + tagIndex + 1;
				float refIndex = atof(index);
				mats[mats.size() - 1].constants[INDEX_Ni] = refIndex;
				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_OPTICAL_DENSITY;
				break;
			}
			case _map_Ka: //for these ones forward, will also need to set the tex specific flags too in mapProperties; not checking for options at the moment
			{
				string_view relative = lineView.substr(tagIndex + 1, lineView.length() - (tagIndex + 1));
				relative = relative.substr(0, relative.find_first_of(' ')); //remove any white spaces at the end of the line
				wstring fullPath(relative.begin(), relative.end());
				fullPath = basePath + fullPath;

				ImageLoader::LoadImageFromFile(fullPath, mats[mats.size() - 1].maps[INDEX_Map_Ka]); //load texture data

				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_MAP_AMBIENT; //set both properties
				mats[mats.size() - 1].mapProperties[INDEX_Map_Ka] |= MATERIAL_PROPERTIES_MAP_AMBIENT;
				break;
			}
			case _map_Kd:
			{
				string_view relative = lineView.substr(tagIndex + 1, lineView.length() - (tagIndex + 1));
				relative = relative.substr(0, relative.find_first_of(' ')); //remove any white spaces at the end of the line
				wstring fullPath(relative.begin(), relative.end());
				fullPath = basePath + fullPath;

				ImageLoader::LoadImageFromFile(fullPath, mats[mats.size() - 1].maps[INDEX_Map_Kd]); //load texture data

				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_MAP_DIFFUSE; //set both properties
				mats[mats.size() - 1].mapProperties[INDEX_Map_Kd] |= MATERIAL_PROPERTIES_MAP_DIFFUSE;
				break;
			}
			case _map_Ks:
			{
				string_view relative = lineView.substr(tagIndex + 1, lineView.length() - (tagIndex + 1));
				relative = relative.substr(0, relative.find_first_of(' ')); //remove any white spaces at the end of the line
				wstring fullPath(relative.begin(), relative.end());
				fullPath = basePath + fullPath;

				ImageLoader::LoadImageFromFile(fullPath, mats[mats.size() - 1].maps[INDEX_Map_Ks]); //load texture data

				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_MAP_SPECULAR_COLOR; //set both properties
				mats[mats.size() - 1].mapProperties[INDEX_Map_Ks] |= MATERIAL_PROPERTIES_MAP_SPECULAR_COLOR;
				break;
			}
			case _map_Ns:
			{
				string_view relative = lineView.substr(tagIndex + 1, lineView.length() - (tagIndex + 1));
				relative = relative.substr(0, relative.find_first_of(' ')); //remove any white spaces at the end of the line
				wstring fullPath(relative.begin(), relative.end());
				fullPath = basePath + fullPath;

				ImageLoader::LoadImageFromFile(fullPath, mats[mats.size() - 1].maps[INDEX_Map_Ns]); //load texture data

				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_MAP_SPECULAR_HIGHLIGHT; //set both properties
				mats[mats.size() - 1].mapProperties[INDEX_Map_Ns] |= MATERIAL_PROPERTIES_MAP_SPECULAR_HIGHLIGHT;
				break;
			}
			case _map_d:
			{
				string_view relative = lineView.substr(tagIndex + 1, lineView.length() - (tagIndex + 1));
				relative = relative.substr(0, relative.find_first_of(' ')); //remove any white spaces at the end of the line
				wstring fullPath(relative.begin(), relative.end());
				fullPath = basePath + fullPath;

				ImageLoader::LoadImageFromFile(fullPath, mats[mats.size() - 1].maps[INDEX_Map_d]); //load texture data

				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_MAP_DISSOLVE; //set both properties
				mats[mats.size() - 1].mapProperties[INDEX_Map_d] |= MATERIAL_PROPERTIES_MAP_DISSOLVE;
				break;
			}
			case _map_Bump: //both _map_bump and _bump do the same thing
			{
				string_view relative = lineView.substr(tagIndex + 1, lineView.length() - (tagIndex + 1));
				relative = relative.substr(0, relative.find_first_of(' ')); //remove any white spaces at the end of the line
				wstring fullPath(relative.begin(), relative.end());
				fullPath = basePath + fullPath;

				ImageLoader::LoadImageFromFile(fullPath, mats[mats.size() - 1].maps[INDEX_Map_bump]); //load texture data

				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_MAP_BUMP; //set both properties
				mats[mats.size() - 1].mapProperties[INDEX_Map_bump] |= MATERIAL_PROPERTIES_MAP_BUMP;
				break;
			}
			case _bump:
			{
				string_view relative = lineView.substr(tagIndex + 1, lineView.length() - (tagIndex + 1));
				relative = relative.substr(0, relative.find_first_of(' ')); //remove any white spaces at the end of the line
				wstring fullPath(relative.begin(), relative.end());
				fullPath = basePath + fullPath;

				ImageLoader::LoadImageFromFile(fullPath, mats[mats.size() - 1].maps[INDEX_Map_bump]); //load texture data

				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_MAP_BUMP; //set both properties
				mats[mats.size() - 1].mapProperties[INDEX_Map_bump] |= MATERIAL_PROPERTIES_MAP_BUMP;
				break;
			}
			case _disp:
			{
				string_view relative = lineView.substr(tagIndex + 1, lineView.length() - (tagIndex + 1));
				relative = relative.substr(0, relative.find_first_of(' ')); //remove any white spaces at the end of the line
				wstring fullPath(relative.begin(), relative.end());
				fullPath = basePath + fullPath;

				ImageLoader::LoadImageFromFile(fullPath, mats[mats.size() - 1].maps[INDEX_Map_disp]); //load texture data

				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_MAP_DISPLACEMENT; //set both properties
				mats[mats.size() - 1].mapProperties[INDEX_Map_disp] |= MATERIAL_PROPERTIES_MAP_DISPLACEMENT;
				break;
			}
			case _decal:
			{
				string_view relative = lineView.substr(tagIndex + 1, lineView.length() - (tagIndex + 1));
				relative = relative.substr(0, relative.find_first_of(' ')); //remove any white spaces at the end of the line
				wstring fullPath(relative.begin(), relative.end());
				fullPath = basePath + fullPath;

				ImageLoader::LoadImageFromFile(fullPath, mats[mats.size() - 1].maps[INDEX_Map_decal]); //load texture data

				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_MAP_DECAL; //set both properties
				mats[mats.size() - 1].mapProperties[INDEX_Map_decal] |= MATERIAL_PROPERTIES_MAP_DECAL;
				break;
			}
			case _refl: //doesn't always have a specified type option (but often can)
			{
				string_view relative = lineView.substr(tagIndex + 1, lineView.length() - (tagIndex + 1));
				relative = relative.substr(0, relative.find_first_of(' ')); //remove any white spaces at the end of the line
				wstring fullPath(relative.begin(), relative.end());
				fullPath = basePath + fullPath;

				ImageLoader::LoadImageFromFile(fullPath, mats[mats.size() - 1].maps[INDEX_Map_refl]); //load texture data

				mats[mats.size() - 1].properties |= MATERIAL_PROPERTIES_MAP_REFLECTION; //set both properties
				mats[mats.size() - 1].mapProperties[INDEX_Map_refl] |= MATERIAL_PROPERTIES_MAP_REFLECTION;
				break;
			}
			default: //unsupported tag
				break;
			}

			return true;
		}

		size_t GetTag(const char* line, _int64& value) {
			value = 0;
			size_t index = 0;
			while (*line > ' ') { //also excludes \0 and some other special characters
				value = (value << 8) + *line++;
				index++;
			}
			return index;
		}

		void ReadV3(const string_view& line, const size_t& tag, POINT& vec) {
			const char* index = line.data() + tag + 1;

			float v1 = atof(index);
			float v2 = atof(index);
			float v3 = atof(index);

			vec = POINT(v1, v2, v3);
		}
		void ReadV2(const string_view& line, const size_t& tag, TEX& vec) {
			const char* index = line.data() + tag + 1; //+1 for white space between tag and data

			float v1 = atof(index);
			float v2 = atof(index);

			vec = TEX(v1, 1.0f - v2); //1.0f - to account for the fact y is flipped when rendering
		}

		//can avoid creating duplicate strings by adding index to the c_str -- should do wherever possible
		//need to account for the fact indices start at 1
		IND SplitFace(const char*& ind) {
			int v = atou(ind) - 1;
			ind++;
			int vt = atou(ind) - 1;
			ind++;
			int vn = atou(ind) - 1;

			return IND(v, vt, vn);
		}

		//faster conversion to float
		float atof(const char*& p)
		{
			float sign, value;

			//account for possible single whitespace
			if (*p == ' ') {
				p++;
			}

			// Get sign, if any (not checking for + since it is not expected).
			sign = 1.0f;
			if (*p == '-') {
				sign = -1.0f;
				p += 1;
			}

			//Get digits before decimal point or exponent, if any.
			for (value = 0.0f; valid_digit(*p); p += 1) {
				value = value * 10.0f + (*p - '0');
			}

			//Get digits after decimal point, if any.	
			p += 1;
			float decimal = 0.0f, weight = 1.0f;
			for (; valid_digit(*p); p += 1) {
				weight *= 0.1f;
				decimal += weight * (*p - '0');
			}

			//Return signed floating point result (not expecting exponential E notation).
			return sign * (value + decimal);
		}

		//not expecting negatives (the obj parser doesn't support this anyways, and this is only used for converting indices in the face list)
		int atou(const char*& p)
		{
			int val = 0;
			while (valid_digit(*p)) {
				val = val * 10 + (*p++ - '0');
			}
			return val;
		}

		void operator|=(MATERIAL_PROPERTIES& left, MATERIAL_PROPERTIES right) {
			left = (MATERIAL_PROPERTIES)(left | right);
		}
	}
}