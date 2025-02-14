#pragma once

#include "Camera.h"

namespace Razor 
{

	class FPSCamera : public Camera
	{
	public:
		FPSCamera(Window* window);
		~FPSCamera();

		virtual void updateVectors();
		virtual void update(double dt) override;

		virtual void onEvent(Window* window) override;
		virtual void onKeyPressed(Direction dir) override;
		virtual void onMouseMoved(glm::vec2& pos, bool constrain = true) override;
		virtual void onMouseScrolled(glm::vec2& offset) override;
		virtual void onMouseDown(int button) override;
		virtual void onMouseUp(int button) override;
		virtual void onWindowResized(const glm::vec2& size) override;

	private:
		float sensitivity;
	
		float view_friction;
		float move_friction;

		glm::vec2 mouse_offset;
		glm::vec2 mouse;
		bool constrain_pitch;
	};

}
