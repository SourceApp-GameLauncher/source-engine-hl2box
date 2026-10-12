//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: HL2SB (2026-10-07): GMod private screenspace bloom shader, the
//          composite pass behind DrawBloom( ) from lua/postprocess/bloom.lua
//          (materials/pp/bloom.vmt).  Reference behavior: samples the base
//          framebuffer texture, blends each channel through the per-channel
//          $levelr/$levelg/$levelb gains with the $colormul crossfade, and
//          renders additively (ONE/ONE).  Compiled programs ship verbatim as
//          shaders/fxc/g_bloom_{vs20,ps20}.vcs.
//
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"

BEGIN_VS_SHADER_FLAGS( g_bloom, "GMod bloom composite", SHADER_NOT_EDITABLE )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( LEVELR, SHADER_PARAM_TYPE_FLOAT, "0.5", "Red Level" )
		SHADER_PARAM( LEVELG, SHADER_PARAM_TYPE_FLOAT, "0.5", "Green Level" )
		SHADER_PARAM( LEVELB, SHADER_PARAM_TYPE_FLOAT, "0.5", "Blue Level" )
		SHADER_PARAM( COLORMUL, SHADER_PARAM_TYPE_FLOAT, "1.0", "Color Multiplier" )
	END_SHADER_PARAMS

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		LoadTexture( BASETEXTURE );
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );

			int fmt = VERTEX_POSITION;
			pShaderShadow->VertexShaderVertexFormat( fmt, 1, 0, 0 );

			pShaderShadow->SetVertexShader( "g_bloom_vs20", 0 );
			pShaderShadow->SetPixelShader( "g_bloom_ps20", 0 );

			// Reference state: additive composite (ONE, ONE).
			pShaderShadow->EnableBlending( true );
			pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
		}
		DYNAMIC_STATE
		{
			// One parameter per constant register (c0 levelr, c1 levelg,
			// c2 levelb, c3 colormul) -- the compiled ps20 reads c0.x..c3.x
			// as four independent gains.  Packing all four into c0 left
			// c1..c3 holding whatever the previous draw had parked there,
			// so the composite quad was added with garbage gains.
			float flR[4] = { params[LEVELR]->GetFloatValue(), 0.0f, 0.0f, 0.0f };
			float flG[4] = { params[LEVELG]->GetFloatValue(), 0.0f, 0.0f, 0.0f };
			float flB[4] = { params[LEVELB]->GetFloatValue(), 0.0f, 0.0f, 0.0f };
			float flM[4] = { params[COLORMUL]->GetFloatValue(), 0.0f, 0.0f, 0.0f };
			pShaderAPI->SetPixelShaderConstant( 0, flR, 1 );
			pShaderAPI->SetPixelShaderConstant( 1, flG, 1 );
			pShaderAPI->SetPixelShaderConstant( 2, flB, 1 );
			pShaderAPI->SetPixelShaderConstant( 3, flM, 1 );

			BindTexture( SHADER_SAMPLER0, BASETEXTURE, -1 );
			pShaderAPI->SetVertexShaderIndex( 0 );
		}
		Draw();
	}
END_SHADER
