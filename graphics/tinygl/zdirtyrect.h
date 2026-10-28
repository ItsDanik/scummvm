/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef GRAPHICS_TINYGL_ZRECT_H
#define GRAPHICS_TINYGL_ZRECT_H

#include "common/types.h"
#include "common/rect.h"
#include "common/array.h"

#include "graphics/tinygl/zblit.h"

namespace TinyGL {

namespace Internal {
void *allocateFrame(int size);
}

struct GLContext;
struct GLVertex;
struct GLTexture;

struct GLTextureEnvArgument {
	GLTextureEnvArgument();

	uint
		sourceRGB,
		operandRGB,
		sourceAlpha,
		operandAlpha;
};

struct GLTextureEnv {
	GLTextureEnv();
	bool isDefault() const;

	uint envMode, combineRGB, combineAlpha;
	byte constA, constR, constG, constB;
	GLTextureEnvArgument arg0, arg1;
};

class DrawCall {
public:

	enum DrawCallType {
		DrawCall_Rasterization,
		DrawCall_Blitting,
		DrawCall_Clear
	};

	DrawCall(DrawCallType type) : _type(type) { }
	virtual ~DrawCall() { }
	bool operator==(const DrawCall &other) const;
	bool operator!=(const DrawCall &other) const {
		return !(*this == other);
	}
	virtual void execute(bool restoreState, const Common::Rect *clippingRectangle = nullptr) const = 0;
	DrawCallType getType() const { return _type; }
	virtual const Common::Rect getDirtyRegion() const { return _dirtyRegion; }
	// presentBufferSimple(): whether it can be one of the frame's first draw
	// calls, the base, which give the color and z buffers what they have
	// whatever they had before (then what it does goes into signature;
	// colorSet and zSet say whether the calls before it have overwritten
	// the whole of a buffer), and the part of the buffers it may write
	virtual bool baseSignature(Common::Array<uint32> &signature, bool &colorSet, bool &zSet) const { return false; }
	virtual Common::Rect drawnRegion() = 0;
protected:
	Common::Rect _dirtyRegion;
private:
	DrawCallType _type;
};

class ClearBufferDrawCall : public DrawCall {
public:
	ClearBufferDrawCall(bool clearZBuffer, int zValue, bool clearColorBuffer, int rValue, int gValue, int bValue, bool clearStencilBuffer, int stencilValue);
	virtual ~ClearBufferDrawCall() { }
	bool operator==(const ClearBufferDrawCall &other) const;
	void execute(bool restoreState, const Common::Rect *clippingRectangle = nullptr) const override;
	bool baseSignature(Common::Array<uint32> &signature, bool &colorSet, bool &zSet) const override;
	Common::Rect drawnRegion() override;
	// Leave the z buffer or the color buffer alone (presentBufferSimple())
	void skipBuffer(bool zBuffer) {
		if (zBuffer)
			_clearZBuffer = false;
		else
			_clearColorBuffer = false;
	}

	void *operator new(size_t size) {
		return Internal::allocateFrame(size);
	}

	void operator delete(void *p) { }
private:
	bool _clearZBuffer, _clearColorBuffer, _clearStencilBuffer;
	int _rValue, _gValue, _bValue, _zValue, _stencilValue;
	struct ClearBufferState {
		bool enableScissor;
		int scissor[4];

		bool operator==(const ClearBufferState &other) const {
			return
				enableScissor == other.enableScissor &&
				scissor[0] == other.scissor[0] &&
				scissor[1] == other.scissor[1] &&
				scissor[2] == other.scissor[2] &&
				scissor[3] == other.scissor[3];
		}
	};

	// (GLContext::presentBufferSimple() keeps the context's own state with them)
	friend struct GLContext;
	static ClearBufferState captureState();
	static void applyState(const ClearBufferState &state, const Common::Rect *clippingRectangle);

	ClearBufferState _clearState;
};

// Encapsulate a rasterization call: it might execute either a triangle or line rasterization.
class RasterizationDrawCall : public DrawCall {
public:
	RasterizationDrawCall();
	virtual ~RasterizationDrawCall() { }
	bool operator==(const RasterizationDrawCall &other) const;
	void execute(bool restoreState, const Common::Rect *clippingRectangle = nullptr) const override;
	Common::Rect drawnRegion() override;
	// GLContext::glopEnd() without dirty rectangles: a draw that comes right
	// after this one, with the same state, is drawn with it (one state to
	// apply and one region for both; Grim Fandango draws each face of its
	// models on its own): whether the context's state is this one's, and
	// the context's vertices added to it
	bool takesContextDraw() const;
	void addContextDraw();

	void *operator new(size_t size) {
		return Internal::allocateFrame(size);
	}

	void operator delete(void *p) { }
private:
	void computeDirtyRegion();
	void addRegion(GLVertex *vertex, int vertexCount);
	// the context's begin_type, vertex and vertex_cnt drawn
	static void drawPrimitive(GLContext *c);
	typedef void (*gl_draw_triangle_func_ptr)(GLContext *c, TinyGL::GLVertex *p0, TinyGL::GLVertex *p1, TinyGL::GLVertex *p2);
	int _vertexCount;
	GLVertex *_vertex;
	gl_draw_triangle_func_ptr _drawTriangleFront, _drawTriangleBack;
	// the draws added to the first one (addContextDraw()), in their order
	struct Primitive {
		int beginType;
		int vertexCount;
		GLVertex *vertex;
		Primitive *next;
	};
	Primitive *_more;
	Primitive **_moreEnd;

	struct RasterizationState {
		bool enableScissor;
		int scissor[4];
		int beginType;
		int currentFrontFace;
		int cullFaceEnabled;
		bool colorMaskRed;
		bool colorMaskGreen;
		bool colorMaskBlue;
		bool colorMaskAlpha;
		bool depthTestEnabled;
		int depthFunction;
		int depthWriteMask;
		bool texture2DEnabled;
		int currentShadeModel;
		int polygonModeBack;
		int polygonModeFront;
		int lightingEnabled;
		bool enableBlending;
		int sfactor;
		int dfactor;
		int textureVersion;
		int offsetStates;
		float offsetFactor;
		float offsetUnits;
		float viewportTranslation[3];
		float viewportScaling[3];
		bool alphaTestEnabled;
		int alphaFunc;
		int alphaRefValue;
		bool stencilTestEnabled;
		int stencilTestFunc;
		byte stencilValue;
		byte stencilMask;
		byte stencilWriteMask;
		int stencilSfail;
		int stencilDpfail;
		int stencilDppass;
		bool polygonStippleEnabled;
		byte polygonStipplePattern[128];
		uint32 stippleColor;
		bool two_color_stipple_enabled;
		GLTexture *texture;
		uint wrapS, wrapT;
		GLTextureEnv textureEnv;
		bool fogEnabled;
		float fogColorR;
		float fogColorG;
		float fogColorB;

		bool operator==(const RasterizationState &other) const;
	};

	RasterizationState _state;

	// (GLContext::presentBufferSimple() keeps the context's own state with them)
	friend struct GLContext;
	static RasterizationState captureState();
	static void applyState(const RasterizationState &state, const Common::Rect *clippingRectangle);
};

// Encapsulate a blit call: it might execute either a color buffer or z buffer blit.
class BlittingDrawCall : public DrawCall {
public:
	enum BlittingMode {
		BlitMode_Regular,
		BlitMode_Fast,
		BlitMode_ZBuffer
	};

	BlittingDrawCall(BlitImage *image, const BlitTransform &transform, BlittingMode blittingMode);
	virtual ~BlittingDrawCall();
	bool operator==(const BlittingDrawCall &other) const;
	void execute(bool restoreState, const Common::Rect *clippingRectangle = nullptr) const override;

	BlittingMode getBlittingMode() const { return _mode; }
	// Whether it writes every pixel of the whole frame buffer, without
	// reading them: of the z buffer (zBuffer true) or of the color buffer
	bool overwritesBuffer(bool &zBuffer) const;
	bool baseSignature(Common::Array<uint32> &signature, bool &colorSet, bool &zSet) const override;
	Common::Rect drawnRegion() override;

	void *operator new(size_t size) {
		return Internal::allocateFrame(size);
	}

	void operator delete(void *p) { }
private:
	void computeDirtyRegion();
	BlitImage *_image;
	BlitTransform _transform;
	BlittingMode _mode;
	int _imageVersion;

	struct BlittingState {
		bool enableScissor;
		int scissor[4];
		bool enableBlending;
		int sfactor, dfactor;
		bool alphaTest;
		int alphaFunc, alphaRefValue;
		int depthTestEnabled;

		bool operator==(const BlittingState &other) const {
			return
				enableScissor == other.enableScissor &&
				scissor[0] == other.scissor[0] &&
				scissor[1] == other.scissor[1] &&
				scissor[2] == other.scissor[2] &&
				scissor[3] == other.scissor[3] &&
				enableBlending == other.enableBlending &&
				sfactor == other.sfactor &&
				dfactor == other.dfactor &&
				alphaTest == other.alphaTest &&
				alphaFunc == other.alphaFunc &&
				alphaRefValue == other.alphaRefValue &&
				depthTestEnabled == other.depthTestEnabled;
		}
	};

	// (GLContext::presentBufferSimple() keeps the context's own state with them)
	friend struct GLContext;
	static BlittingState captureState();
	static void applyState(const BlittingState &state, const Common::Rect *clippingRectangle);

	BlittingState _blitState;
};

} // end of namespace TinyGL

#endif
