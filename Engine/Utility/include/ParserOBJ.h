#pragma once

#include <vector>
#include <windows.h>
#include <string>

#include "ImageLoader.h"

namespace Parser {
	namespace OBJ {
		struct POINT {
			POINT(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
			POINT() : x(0.0f), y(0.0f), z(0.0f) {}
			float x;
			float y;
			float z;
		};
		//inherits from NORMAL so that the same subroutine can be used to fill the two of them when parsing
		struct VERTEX : public POINT {
			VERTEX(float X, float Y, float Z, float W = 1.0f) : POINT(X, Y, Z), w(W) {}
			VERTEX() : POINT(), w(1.0f) {}
			float w;
		};
		struct TEX {
			TEX(float U, float V) : u(U), v(V) {}
			TEX() : u(0.0f), v(0.0f) {}
			float u;
			float v;
		};
		struct IND {
			IND(int V, int VT, int VN) : v(V), vn(VN), vt(VT) {} //change this and all references in d3d to move to v/vt/vn, since this is the order in the file
			IND() : v(0), vn(0), vt(0) {}
			int v;
			int vn;
			int vt;
		};

		enum MATERIAL_PROPERTIES {
			MATERIAL_PROPERTIES_AMBIENT = 0x80000000, //first byte; marks what base values have been defined in the material (if a mapped version is also marked, then that will take precedence)
			MATERIAL_PROPERTIES_DIFFUSE = 0x40000000,
			MATERIAL_PROPERTIES_SPECULAR = 0x20000000,
			MATERIAL_PROPERTIES_DISSOLVE = 0x10000000,
			MATERIAL_PROPERTIES_TR_FILTER = 0x8000000,
			MATERIAL_PROPERTIES_OPTICAL_DENSITY = 0x4000000,

			MATERIAL_PROPERTIES_MAP_REFLECTION = 0x1000000,
			MATERIAL_PROPERTIES_MAP_AMBIENT = 0x800000, //second byte; marks which types of maps have been defined in the material
			MATERIAL_PROPERTIES_MAP_DIFFUSE = 0x400000,
			MATERIAL_PROPERTIES_MAP_SPECULAR_COLOR = 0x200000,
			MATERIAL_PROPERTIES_MAP_SPECULAR_HIGHLIGHT = 0x100000,
			MATERIAL_PROPERTIES_MAP_DISSOLVE = 0x80000,
			MATERIAL_PROPERTIES_MAP_BUMP = 0x40000,
			MATERIAL_PROPERTIES_MAP_DISPLACEMENT = 0x20000,
			MATERIAL_PROPERTIES_MAP_DECAL = 0x10000,

			MATERIAL_PROPERTIES_OPT_BLEND_U = 0x8000, //third byte; mark options that are used at least once by one of the textures defined in the material (acts as a flag to quickly know whether more complex shaders are needed or not)
			MATERIAL_PROPERTIES_OPT_BLEND_V = 0x4000,
			MATERIAL_PROPERTIES_OPT_BOOST_MIP_MAP = 0x2000,
			MATERIAL_PROPERTIES_OPT_TEX_MODIFY_BASE_GAIN = 0x1000,
			MATERIAL_PROPERTIES_OPT_TEX_OFFSET_ORIGIN = 0x800,
			MATERIAL_PROPERTIES_OPT_TEX_SCALE = 0x400,
			MATERIAL_PROPERTIES_OPT_TEX_TURBULENCE = 0x200,
			MATERIAL_PROPERTIES_OPT_TEX_CLAMP = 0x100,

			MATERIAL_PROPERTIES_OPT_TEX_COLOR_CORRECTION = 0x80, //fourth byte;
			MATERIAL_PROPERTIES_OPT_BUMP_MULTIPLIER = 0x40,
			MATERIAL_PROPERTIES_OPT_CHANNEL_SPECIFIED = 0x20, //(imfchan)
			MATERIAL_PROPERTIES_REFLECTION_TYPE_SPHERE = 0x10,
			MATERIAL_PROPERTIES_REFLECTION_TYPE_CUBE = 0x8,
			MATERIAL_PROPERTIES_USES_EXTENSIONS = 0x4,

			MATERIAL_PROPERTIES_TR_FILTER_SPECTRAL = 0x2, //uncommon & not supported atm

			MATERIAL_NOT_PROPERTY = 0x00,
			MATERIAL_PROPERTY_NONE = 0x00
		};
		void operator|=(MATERIAL_PROPERTIES& left, MATERIAL_PROPERTIES right);

		//supported extensions (such as physically based rendering)
		enum EXTENSIONS {

			EXTENSIONS_NONE = 0x00,
			EXTENSIONS_NOT_SUPPORTED = 0x00,
		};		
		struct EXTENSION_DATA {
			EXTENSIONS EXTs = EXTENSIONS_NONE;

		};

		enum INDEX {
			INDEX_Ns = 0x00,
			INDEX_d = 0x01,
			INDEX_Ni = 0x02,

			INDEX_Ka = 0x00,
			INDEX_Kd = 0x01,
			INDEX_Ks = 0x02,
			INDEX_Tf = 0x03,

			INDEX_Map_Ka = 0x00,
			INDEX_Map_Kd = 0x01,
			INDEX_Map_Ks = 0x02,
			INDEX_Map_Ns = 0x03,
			INDEX_Map_d = 0x04,
			INDEX_Map_bump = 0x05,
			INDEX_Map_disp = 0x06,
			INDEX_Map_decal = 0x07,
			INDEX_Map_refl = 0x08,
		};

		struct MATERIAL {
			MATERIAL_PROPERTIES properties = MATERIAL_PROPERTY_NONE; //for quickly assessing everything used in a material
			std::vector<int> start; //list for all index lists that may use this (ie. for duplicate usemtl)
			std::vector<int> length; //processed at the end (once the start of all the materials have been calculated)

			std::wstring name;

			//the values and maps can be indexed using INDEX
			float constants[3]; 
			POINT colors[4]; 

			MAP maps[9]; //up to 9 maps depending on the material properties
			MATERIAL_PROPERTIES mapProperties[9] = { MATERIAL_PROPERTY_NONE }; //individual texture properties; follows the order of the maps[] array, but can also be idenified using the material property

			EXTENSION_DATA extensions;
		};

		//should also calculate normals if needed, and provide a default texture if one is needed (ie. if there are no texture coordinates, or no texture maps)
		bool Load(std::wstring& filePath, std::vector<IND>& ind, std::vector<VERTEX>& v, std::vector<POINT>& vn, std::vector<TEX>& vt, std::vector<MATERIAL> &mats);
	}
}