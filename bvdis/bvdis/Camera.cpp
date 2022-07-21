#include "Camera.h"
#include <cmath>

namespace nv {
	namespace dx = DirectX;
	constexpr auto PI = 3.14159265358979;

	Camera::Camera()
	{
		Reset();
	}

	dx::XMMATRIX Camera::GetMatrix() {
		auto UpDirection = dx::XMVectorSet(0, 1, 0, 0);  // 返回向量(x, y, z, w)
		auto ForwardBase = dx::XMVectorSet(0, 0, 1, 0);  // 返回向量(x, y, z, w)

		auto pos = dx::XMVectorSet(cameraPosX, cameraPosY, cameraPosZ, 0);  // 返回向量(x, y, z, w)
		auto look = dx::XMVector3Transform(
			ForwardBase, 
			dx::XMMatrixRotationRollPitchYaw(-cameraRollX, -cameraRollY, cameraRollZ) // 按照先绕Z轴，然后X轴，最后Y轴的顺序得到旋转矩阵(弧度，逆时针)
		); // 3D向量与矩阵相乘

		auto target = dx::XMVectorAdd(pos, look);
		auto m = dx::XMMatrixLookAtLH(
			pos, // 摄像机位置
			target,
			UpDirection // 摄像机头顶向量，通常就是y轴
		) * dx::XMMatrixPerspectiveLH(viewWidth, viewHeight, nearZ, farZ); // 透视投影矩阵

		return m;
	}
	void Camera::MoveForward(float step)
	{
		Move({ 0, 0, 1 }, step);
	}
	void Camera::MoveBack(float step)
	{
		Move({ 0, 0, -1 }, step);
	}
	void Camera::MoveLeft(float step)
	{
		Move({ 1, 0, 0 }, step);
	}
	void Camera::MoveRight(float step)
	{
		Move({ -1, 0, 0 }, step);
	}
	void Camera::TurnLeftRight(float step)
	{
		cameraRollY -= step;
	}
	void Camera::TurnUpDown(float step)
	{
		cameraRollX -= step;
	}
	void Camera::Reset()
	{
		cameraPosY = 0, cameraPosX = 0, cameraPosZ = -2;
		cameraRollX = 0, cameraRollY = 0, cameraRollZ = 0;
		viewWidth = 0.5;
		viewHeight = 0.5;
		nearZ = 0.5;
		farZ = 40;
	}
	void Camera::ToBack() {
		cameraPosY = 0, cameraPosX = 0, cameraPosZ = 2;
		cameraRollX = 0, cameraRollY = PI, cameraRollZ = 0;
		viewWidth = 0.5;
		viewHeight = 0.5;
		nearZ = 0.5;
		farZ = 40;
	}
	void Camera::Move(dx::XMFLOAT3 translation, float speed)
	{
		dx::XMStoreFloat3(&translation, dx::XMVector3Transform(
			dx::XMLoadFloat3(&translation),
			dx::XMMatrixRotationRollPitchYaw(cameraRollX, cameraRollY, 0) *
			dx::XMMatrixScaling(speed, speed, speed)
		));

		cameraPosX -= translation.x;
		// cameraPosY -= translation.y; // 可以使摄像机Y轴移动，就是可以飞上天的意思
		cameraPosZ += translation.z;
	}
}