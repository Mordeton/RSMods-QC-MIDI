#pragma once
#include <format>
#include "GdiplusManager.hpp"

#include "../Mods/CollectColors.hpp"
#include "../Mods/ExtendedRangeMode.hpp"

namespace D3DHelper {
	inline char dlldir[320];
	inline char* GetDirFile(char* name) {
		static char pldir[320];
		strcpy_s(pldir, dlldir);
		strcat_s(pldir, name);
		return pldir;
	}

	inline void Log(const char* fmt, ...)
	{
		if (!fmt)	return;

		char		text[4096];
		va_list		ap;
		va_start(ap, fmt);
		vsprintf_s(text, fmt, ap);
		va_end(ap);

		std::ofstream logfile;
		logfile.open(GetDirFile((PCHAR)"log.txt"), std::ios::app);
		if (logfile.is_open() && text)	logfile << text << std::endl;
		logfile.close();
	}
}

struct Mesh {
	UINT Stride;
	UINT PrimCount;
	UINT NumVertices;

	Mesh(UINT s, UINT p, UINT n) {
		Stride = s;
		PrimCount = p;
		NumVertices = n;
	}

	bool operator ==(const Mesh& meshB) const = default;
};

struct ThiccMesh {
	UINT Stride;
	UINT PrimCount;
	UINT NumVertices;
	UINT StartIndex;
	UINT StartRegister;
	UINT PrimType;
	UINT DeclType;
	UINT VectorCount;
	UINT NumElements;

	ThiccMesh(UINT s, UINT p, UINT n, UINT si, UINT mSR, UINT pt, UINT dt, UINT mvc, UINT nm) {
		Stride = s;
		PrimCount = p;
		NumVertices = n;
		StartIndex = si;
		StartRegister = mSR;
		PrimType = pt;
		DeclType = dt;
		VectorCount = mvc;
		NumElements = nm;
	}

	std::string ToString() const
	{
		return std::format(
			"{{ {},{},{},{},{},{},{},{},{} }},",
			Stride,
			PrimCount,
			NumVertices,
			StartIndex,
			StartRegister,
			PrimType,
			DeclType,
			VectorCount,
			NumElements
		);
	}

	bool operator ==(const ThiccMesh& meshB) const = default;
};

inline bool IsExtraRemoved(std::span<const ThiccMesh> list, const ThiccMesh& mesh)
{
	return std::ranges::find(list, mesh) != list.end();
}


inline bool IsToBeRemoved(std::span<const Mesh> list, const Mesh& mesh)
{
	return std::ranges::find(list, mesh) != list.end();
}