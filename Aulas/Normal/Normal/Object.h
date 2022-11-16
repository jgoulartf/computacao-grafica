/**********************************************************************************
// Object (Arquivo de Cabeçalho)
//
// Criação:     14 Out 2022
// Atualização: 14 Out 2022
// Compilador:  Visual C++ 2022
//
// Descrição:   Define armazenamento para objeto de uma cena
//
**********************************************************************************/

#ifndef DXUT_OBJECT_H_
#define DXUT_OBJECT_H_

#include <d3d12.h>
#include "Types.h"
#include "Mesh.h"
#include <DirectXMath.h>
using namespace DirectX;

struct Object
{
	XMFLOAT4X4 world;		// matriz de mundo
	uint cbIndex;			// índice para o constant buffer
	SubMesh mesh;			// malha de vértices

	Object();				// construtor
};

#endif
