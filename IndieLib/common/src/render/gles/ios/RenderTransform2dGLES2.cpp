/*****************************************************************************************
 * File: RenderTransform2dOpenGL.cpp
 * Desc: Transformations applied before blitting a 2d object usind OpenGL
 *****************************************************************************************/

/*
IndieLib 2d library Copyright (C) 2005 Javier L�pez L�pez (info@pixelartgames.com)
THIS FILE IS AN ADDITIONAL FILE ADDED BY Miguel Angel Qui�ones (2011) (mail:m.quinones.garcia@gmail.com / mikeskywalker007@gmail.com), BUT HAS THE
SAME LICENSE AS THE WHOLE LIBRARY TO RESPECT ORIGINAL AUTHOR OF LIBRARY

This library is free software; you can redistribute it and/or modify it under the
terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this library; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA 02111-1307 USA
*/
#include "Defines.h"

#ifdef INDIERENDER_GLES_IOS
// ----- Includes -----

#include "Global.h"
#include "Defines.h"
#include "OpenGLES2Render.h"
#include "IND_Camera2d.h"
#include "IND_Window.h"

/** @cond DOCUMENT_PRIVATEAPI */

// --------------------------------------------------------------------------------
//							         Public methods
// --------------------------------------------------------------------------------

bool OpenGLES2Render::setViewPort2d(int pX,
                                 int pY,
                                 int pWidth,
                                 int pHeight) {
	// ----- If the region is outside the framebuffer return error -----

	if (pX +  pWidth > _info._fbWidth)  return 0;
	if (pX < 0)                         return 0;
	if (pY + pHeight > _info._fbHeight) return 0;
	if (pY < 0)                         return 0;

	// ----- Viewport characteristics -----

	_info._viewPortX      = pX;
	_info._viewPortY      = pY;
	_info._viewPortWidth  = pWidth;
	_info._viewPortHeight = pHeight;
    _info._viewPortApectRatio = static_cast<float>(pWidth/pHeight);

	//Clear projection matrix
	_shaderProjectionMatrix = IND_Matrix::identity();

	//Define the viewport
	glViewport(static_cast<GLint>(pX),
	           static_cast<GLint>(pY),
	           static_cast<GLsizei>(pWidth),
	           static_cast<GLsizei>(pHeight));

	setDefaultGLState();
	return true;
}


void OpenGLES2Render::setCamera2d(IND_Camera2d *pCamera2d) {
	// ----- Lookat matrix -----
	//Rotate that axes in Z by the camera angle
	//Roll is rotation around the z axis (_look)
	IND_Matrix rollmatrix;
	float rotAngle (pCamera2d->_angle - pCamera2d->_prevAngle);
	_math.matrix4DSetRotationAroundAxis(rollmatrix, rotAngle , pCamera2d->_look);
	_math.transformVector3DbyMatrix4D(pCamera2d->_right, rollmatrix);
	_math.transformVector3DbyMatrix4D(pCamera2d->_up, rollmatrix);
	//Reset camera angle difference
	pCamera2d->setAngle(pCamera2d->_angle);
	
	//Build the view matrix from the transformed camera axis
	IND_Matrix lookatmatrix; 
	_math.matrix4DLookAtMatrixLH(pCamera2d->_right,
	                           pCamera2d->_up,
							   pCamera2d->_look,
							   pCamera2d->_pos,
	                           lookatmatrix);

    
    _cameraMatrix = IND_Matrix::identity();
    
    //------ Zooming -----
	if (pCamera2d->_zoom != 1.0f) {
        //Zoom global scale (around where camera points - screen center)
        IND_Matrix zoom = IND_Matrix();
        _math.matrix4DSetScale(zoom, pCamera2d->_zoom, pCamera2d->_zoom,0.f);
        _math.matrix4DMultiplyInPlace(_cameraMatrix, zoom);
	} 

	//------ Lookat transform -----
    _math.matrix4DMultiplyInPlace(_cameraMatrix, lookatmatrix);
    
	//------ Global point to pixel ratio -----
    IND_Matrix pointPixelScale;
    _math.matrix4DSetScale(pointPixelScale,_info._pointPixelScale, _info._pointPixelScale, 1.0f);
    _math.matrix4DMultiplyInPlace(_cameraMatrix, pointPixelScale);
    
	// ----- Projection Matrix -----
	//Setup a 2d projection (orthogonal)
	perspectiveOrtho(static_cast<float>(_info._viewPortWidth), static_cast<float>(_info._viewPortHeight), 2048.0f, -2048.0f);
}


void OpenGLES2Render::setTransform2d(int pX,
                                  int pY,
                                  float pAngleX,
                                  float pAngleY,
                                  float pAngleZ,
                                  float pScaleX,
                                  float pScaleY,
                                  int pAxisCalX,
                                  int pAxisCalY,
                                  bool pMirrorX,
                                  bool pMirrorY,
                                  int pWidth,
                                  int pHeight,
                                  IND_Matrix *pMatrix) {

	//Temporal holders for all accumulated transforms
	IND_Matrix totalTrans;
	_math.matrix4DSetIdentity(totalTrans);

	//Initialize to identity given matrix, if exists
	if (pMatrix) {
		_math.matrix4DSetIdentity(*pMatrix);
	}
    
	// Translations
	if (pX != 0 || pY != 0) {
		IND_Matrix trans;
		_math.matrix4DSetTranslation(trans,static_cast<float>(pX),static_cast<float>(pY),0.0f);
		_math.matrix4DMultiplyInPlace(totalTrans,trans);
	}

	// Scaling
	if (pScaleX != 1.0f || pScaleY != 1.0f) {
		IND_Matrix scale;
		_math.matrix4DSetScale(scale,pScaleX,pScaleY,0.0f);
		_math.matrix4DMultiplyInPlace(totalTrans,scale);
	}

	// Rotations
	if (pAngleX != 0.0f) {
		IND_Matrix angleX;
		_math.matrix4DSetRotationAroundAxis(angleX,pAngleX,IND_Vector3(1.0f,0.0f,0.0f));
		_math.matrix4DMultiplyInPlace(totalTrans,angleX);
	}

	if (pAngleY != 0.0f) {
		IND_Matrix angleY;
		_math.matrix4DSetRotationAroundAxis(angleY,pAngleY,IND_Vector3(0.0f,1.0f,0.0f));
		_math.matrix4DMultiplyInPlace(totalTrans,angleY);
	}

	if (pAngleZ != 0.0f) {
		IND_Matrix angleZ;
		_math.matrix4DSetRotationAroundAxis(angleZ,pAngleZ,IND_Vector3(0.0f,0.0f,1.0f));
		_math.matrix4DMultiplyInPlace(totalTrans,angleZ);
	}

	// Hotspot - Add hotspot to make all transforms to be affected by it
	if (pAxisCalX != 0 || pAxisCalY != 0) {
		IND_Matrix hotspot;
		_math.matrix4DSetTranslation(hotspot,static_cast<float>(pAxisCalX),static_cast<float>(pAxisCalY),0.0f);
		_math.matrix4DMultiplyInPlace(totalTrans,hotspot);
	}

    // Mirroring (180� rotations) and translation
	if (pMirrorX || pMirrorY) {
		//A mirror is a rotation in desired axis (the actual mirror) and a repositioning because rotation
		//also moves 'out of place' the entity translation-wise
		if (pMirrorX) {
			IND_Matrix mirrorX;
            //After rotation around origin, move back texture to correct place
            _math.matrix4DSetTranslation(mirrorX,
                                         static_cast<float>(pWidth),
                                         0.0f,
                                         0.0f);
			_math.matrix4DMultiplyInPlace(totalTrans,mirrorX);
            
            //Rotate in y, to invert texture
			_math.matrix4DSetRotationAroundAxis(mirrorX,180.0f,IND_Vector3(0.0f,1.0f,0.0f));
			_math.matrix4DMultiplyInPlace(totalTrans,mirrorX);
		}
        
		//A mirror is a rotation in desired axis (the actual mirror) and a repositioning because rotation
		//also moves 'out of place' the entity translation-wise
		if (pMirrorY) {
			IND_Matrix mirrorY;
            //After rotation around origin, move back texture to correct place
            _math.matrix4DSetTranslation(mirrorY,
                                         0.0f,
                                         static_cast<float>(pHeight),
                                         0.0f);
			_math.matrix4DMultiplyInPlace(totalTrans,mirrorY);
            
            //Rotate in x, to invert texture
			_math.matrix4DSetRotationAroundAxis(mirrorY,180.0f,IND_Vector3(1.0f,0.0f,0.0f));
			_math.matrix4DMultiplyInPlace(totalTrans,mirrorY);
		}
	}
	//Cache the change
	_modelToWorld = totalTrans;

	//ModelView matrix will be camera * modelToWorld
    _math.matrix4DMultiply(_cameraMatrix, _modelToWorld, _shaderModelViewMatrix);

	// ----- Return World Matrix (in IndieLib format) ----
	//Transformations have been applied where needed
	if (pMatrix) {
		*pMatrix = totalTrans;
	}
}

void OpenGLES2Render::setTransform2d(IND_Matrix &pMatrix) {
	// ----- Applies the transformation -----
    _math.matrix4DMultiply(_cameraMatrix, pMatrix, _shaderModelViewMatrix);

	//Finally cache the change
	_modelToWorld = pMatrix;
}

void OpenGLES2Render::setIdentityTransform2d ()  {
	// ----- Applies the transformation -----
    _shaderModelViewMatrix = _cameraMatrix;

	//Finally cache the change
	_math.matrix4DSetIdentity(_modelToWorld);
}

void OpenGLES2Render::setRainbow2d(IND_Type pType,
                                bool pCull,
                                bool pMirrorX,
                                bool pMirrorY,
                                IND_Filter pFilter,
                                unsigned char pR,
                                unsigned char pG,
                                unsigned char pB,
                                unsigned char pA,
                                unsigned char pFadeR,
                                unsigned char pFadeG,
                                unsigned char pFadeB,
                                unsigned char pFadeA,
                                IND_BlendingType pSo,
                                IND_BlendingType pDs) {
	//Parameters error correction:
	if (pA > 255) {
		pA = 255;
	}

	//Setup neutral 'blend' for texture stage
	float blendR, blendG, blendB, blendA;
	blendR = blendG = blendB = blendA = 1.0f;

	// ----- Filters -----
    // In GL, texture filtering is applied to the bound texture. From this method we don't know which is the
    // bound texture, so we cache the requested state, so before actually rendering, we could set the state
    // to the bound texture
	int filterType = GL_NEAREST;
	if (IND_FILTER_LINEAR == pFilter) {
		filterType = GL_LINEAR;
	}

    _tex2dState.magFilter = filterType;
    _tex2dState.minFilter = filterType;

	// ----- Back face culling -----
	if (pCull) {
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CW);    
	} else {
		glDisable(GL_CULL_FACE);
	}

	// ----- Back face culling -----
	// Mirroring (180� rotations)
	if (pMirrorX || pMirrorY) {
		if (pMirrorX && !pMirrorY) {
			// Back face culling
			if (pCull) {
				glEnable(GL_CULL_FACE);
				glFrontFace(GL_CCW);    
			} else {
				glDisable(GL_CULL_FACE);
			}
		}

		if (!pMirrorX && pMirrorY) {
			if (pCull) {
				glEnable(GL_CULL_FACE);
				glFrontFace(GL_CCW);    
			} else {
				glDisable(GL_CULL_FACE);
			}
		}
	}

	// ----- Blending -----
	switch (pType) {
        case IND_OPAQUE: {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ZERO);
            
            // Tinting
            if (pR != 255 || pG != 255 || pB != 255) {
				blendR = static_cast<float>(pR) / 255.0f;
				blendG = static_cast<float>(pG) / 255.0f;
				blendB = static_cast<float>(pB) / 255.0f;
                // FIXME: SHADERS
//                glColor4f(blendR, blendG, blendB, blendA);
            }
            
            // Alpha
            if (pA != 255) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				blendA = static_cast<float>(pA) / 255.0f;
                // FIXME: SHADERS
//                glColor4f(blendR, blendG, blendB, blendA);
            }
            
            // Fade to color
            if (pFadeA != 255) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				blendA = static_cast<float>(pFadeA) / 255.0f;
                blendR = static_cast<float>(pFadeR) / 255.0f;
                blendG = static_cast<float>(pFadeG) / 255.0f;
                blendB = static_cast<float>(pFadeB) / 255.0f;
                // FIXME: SHADERS
//                glColor4f(blendR, blendG, blendB, blendA);
            }
            
            if (pSo && pDs) {
                //Alpha blending
            }
        }
            break;
            
        case IND_ALPHA: {
            // Alpha test = OFF
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            // FIXME: SHADERS
//            glColor4f(blendR, blendG, blendB, blendA);
            
            // Tinting
            if (pR != 255 || pG != 255 || pB != 255) {
				blendR = static_cast<float>(pR) / 255.0f;
				blendG = static_cast<float>(pG) / 255.0f;
				blendB = static_cast<float>(pB) / 255.0f;
                // FIXME: SHADERS
//                glColor4f(blendR, blendG, blendB, blendA);
            }
            
            // Alpha
            if (pA != 255) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				blendA = static_cast<float>(pA) / 255.0f;
                // FIXME: SHADERS
//                glColor4f(blendR, blendG, blendB, blendA);
            }
            
            // Fade to color
            if (pFadeA != 255) {
                blendA = static_cast<float>(pFadeA) / 255.0f;
                blendR = static_cast<float>(pFadeR) / 255.0f;
                blendG = static_cast<float>(pFadeG) / 255.0f;
                blendB = static_cast<float>(pFadeB) / 255.0f;
                // FIXME: SHADERS
//                glColor4f(blendR, blendG, blendB, blendA);
            }
            
            if (!pSo || !pDs) {
                //Alpha blending
            } else {
                
            }

	}
	break;
	default: {
	}
	}


}

void OpenGLES2Render::setDefaultGLState() {
    // ----- 2d GLState -----
	//Many defaults are GL_FALSE, but for the sake of explicitly safe operations (and code clearness)
	//I include glDisable explicits
	glDisable(GL_DEPTH_TEST); //No depth testing
    
	// ----- Texturing settings  -----
	//Generally we work with byte-aligned textures.
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
    
    setGLClientStateToTexturing();
}

void OpenGLES2Render::setGLClientStateToPrimitive() {
    // TODO: SHADERS
}

void OpenGLES2Render::setGLClientStateToTexturing() {
    // TODO: SHADERS
}

void OpenGLES2Render::setGLBoundTextureParams() {
    //Texture wrap mode
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,_tex2dState.wrapS);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,_tex2dState.wrapT);
    //By default select fastest texture filter
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, _tex2dState.magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, _tex2dState.minFilter);
}

/** @endcond */

#endif //INDIERENDER_GLES_IOS
