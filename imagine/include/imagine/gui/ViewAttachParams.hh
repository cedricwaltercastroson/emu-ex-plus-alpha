#pragma once

/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

namespace IG
{
class Window;
class ApplicationContext;
}

namespace IG::Gfx
{
class Renderer;
class RendererTask;
}

namespace IG
{

class ViewManager;

struct ViewAttachParams
{
	ViewManager &viewManager;
	Window &window;
	Gfx::RendererTask &rendererTask;

	Gfx::Renderer &renderer() const;
	ApplicationContext appContext() const;
};

}
