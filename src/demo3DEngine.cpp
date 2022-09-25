#include "consoleWindowEngine.h"
#include <fstream>
#include <strstream>
#include <algorithm>
using namespace std;

bool DEBUG_MODE_STATUS;
bool GLOBAL_SPIN_MODE_STATUS;
string MODEL_NAME;


struct point3D
{
	float x = 0;
	float y = 0;
	float z = 0;
	float w = 1; // 4th element to help perform matrix vector multiplication
};

struct triPoly
{
	point3D _point[3];
	wchar_t _symbol;
	short _color;
};

struct triPolyMeshCollection
{
	vector<triPoly> triPolyList;

	bool LoadFromObjectFile(string sFilename)
	{
		/* Capable of reading .obj files from Blender (2.9 as tested) */
		ifstream f(sFilename);
		if (!f.is_open())
			return false;

		/* Local vector to store vertices */
		vector<point3D> verts;

		while (!f.eof())
		{
			char line[128]; /*Assuming the lines from the .obj file are under 128 chars*/
			f.getline(line, 128);

			strstream s;
			s << line;

			char junk;

			if (line[0] == 'v')
			{
				point3D v;
				s >> junk >> v.x >> v.y >> v.z;
				verts.push_back(v);
			}

			if (line[0] == 'f')
			{
				int f[3];
				s >> junk >> f[0] >> f[1] >> f[2];
				triPolyList.push_back({ verts[f[0] - 1], verts[f[1] - 1], verts[f[2] - 1] });
			}
		}
		return true;
	}
};

struct quadMatrix
{
	float _matrix[4][4] = { 0 };
};

class consoleEngine3D : public consoleWindowEngine
{
public:
	consoleEngine3D()
	{
		m_sAppName = L"3D Demo";
	}


private:
	triPolyMeshCollection meshObj;
	quadMatrix matProj;	// Projetion Matrix for conversion from view space to screen space
	point3D vCamera;	// To store location of camera in world space
	point3D vLookDir;	// Vector to store where the camera is pointint
	float fYaw;		// Camera rotation in XZ plane (For FPS)
	float fTheta;	// Spins World transform

	point3D Matrix_MultiplyVector(quadMatrix& m, point3D& i)
	{
		point3D v;
		v.x = i.x * m._matrix[0][0] + i.y * m._matrix[1][0] + i.z * m._matrix[2][0] + i.w * m._matrix[3][0];
		v.y = i.x * m._matrix[0][1] + i.y * m._matrix[1][1] + i.z * m._matrix[2][1] + i.w * m._matrix[3][1];
		v.z = i.x * m._matrix[0][2] + i.y * m._matrix[1][2] + i.z * m._matrix[2][2] + i.w * m._matrix[3][2];
		v.w = i.x * m._matrix[0][3] + i.y * m._matrix[1][3] + i.z * m._matrix[2][3] + i.w * m._matrix[3][3];
		return v;
	}

	quadMatrix Matrix_MakeIdentity()
	{
		quadMatrix matrix;
		matrix._matrix[0][0] = 1.0f;
		matrix._matrix[1][1] = 1.0f;
		matrix._matrix[2][2] = 1.0f;
		matrix._matrix[3][3] = 1.0f;
		return matrix;
	}

	quadMatrix Matrix_MakeRotationX(float fAngleRad)
	{
		quadMatrix matrix;
		matrix._matrix[0][0] = 1.0f;
		matrix._matrix[1][1] = cosf(fAngleRad);
		matrix._matrix[1][2] = sinf(fAngleRad);
		matrix._matrix[2][1] = -sinf(fAngleRad);
		matrix._matrix[2][2] = cosf(fAngleRad);
		matrix._matrix[3][3] = 1.0f;
		return matrix;
	}

	quadMatrix Matrix_MakeRotationY(float fAngleRad)
	{
		quadMatrix matrix;
		matrix._matrix[0][0] = cosf(fAngleRad);
		matrix._matrix[0][2] = sinf(fAngleRad);
		matrix._matrix[2][0] = -sinf(fAngleRad);
		matrix._matrix[1][1] = 1.0f;
		matrix._matrix[2][2] = cosf(fAngleRad);
		matrix._matrix[3][3] = 1.0f;
		return matrix;
	}

	quadMatrix Matrix_MakeRotationZ(float fAngleRad)
	{
		quadMatrix matrix;
		matrix._matrix[0][0] = cosf(fAngleRad);
		matrix._matrix[0][1] = sinf(fAngleRad);
		matrix._matrix[1][0] = -sinf(fAngleRad);
		matrix._matrix[1][1] = cosf(fAngleRad);
		matrix._matrix[2][2] = 1.0f;
		matrix._matrix[3][3] = 1.0f;
		return matrix;
	}

	quadMatrix Matrix_MakeTranslation(float x, float y, float z)
	{
		quadMatrix matrix;
		matrix._matrix[0][0] = 1.0f;
		matrix._matrix[1][1] = 1.0f;
		matrix._matrix[2][2] = 1.0f;
		matrix._matrix[3][3] = 1.0f;
		matrix._matrix[3][0] = x;
		matrix._matrix[3][1] = y;
		matrix._matrix[3][2] = z;
		return matrix;
	}

	quadMatrix Matrix_MakeProjection(float fFovDegrees, float fAspectRatio, float fNear, float fFar)
	{
		float fFovRad = 1.0f / tanf(fFovDegrees * 0.5f / 180.0f * 3.14159f);
		quadMatrix matrix;
		matrix._matrix[0][0] = fAspectRatio * fFovRad;
		matrix._matrix[1][1] = fFovRad;
		matrix._matrix[2][2] = fFar / (fFar - fNear);
		matrix._matrix[3][2] = (-fFar * fNear) / (fFar - fNear);
		matrix._matrix[2][3] = 1.0f;
		matrix._matrix[3][3] = 0.0f;
		return matrix;
	}

	quadMatrix Matrix_MultiplyMatrix(quadMatrix& m1, quadMatrix& m2)
	{
		quadMatrix matrix;
		for (int c = 0; c < 4; c++)
			for (int r = 0; r < 4; r++)
				matrix._matrix[r][c] = m1._matrix[r][0] * m2._matrix[0][c] + m1._matrix[r][1] * m2._matrix[1][c] + m1._matrix[r][2] * m2._matrix[2][c] + m1._matrix[r][3] * m2._matrix[3][c];
		return matrix;
	}

	quadMatrix Matrix_PointAt(point3D& pos, point3D& target, point3D& up)
	{
		// Calculate new forward direction
		point3D newForward = Vector_Sub(target, pos);
		newForward = Vector_Normalise(newForward);

		// Calculate new Up direction
		point3D a = Vector_Mul(newForward, Vector_DotProduct(up, newForward));
		point3D newUp = Vector_Sub(up, a);
		newUp = Vector_Normalise(newUp);

		// New Right direction is easy, its just cross product
		point3D newRight = Vector_CrossProduct(newUp, newForward);

		// Construct Dimensioning and Translation Matrix	
		quadMatrix matrix;
		matrix._matrix[0][0] = newRight.x;	matrix._matrix[0][1] = newRight.y;	matrix._matrix[0][2] = newRight.z;	matrix._matrix[0][3] = 0.0f;
		matrix._matrix[1][0] = newUp.x;		matrix._matrix[1][1] = newUp.y;		matrix._matrix[1][2] = newUp.z;		matrix._matrix[1][3] = 0.0f;
		matrix._matrix[2][0] = newForward.x;	matrix._matrix[2][1] = newForward.y;	matrix._matrix[2][2] = newForward.z;	matrix._matrix[2][3] = 0.0f;
		matrix._matrix[3][0] = pos.x;			matrix._matrix[3][1] = pos.y;			matrix._matrix[3][2] = pos.z;			matrix._matrix[3][3] = 1.0f;
		return matrix;

	}

	quadMatrix Matrix_QuickInverse(quadMatrix& m) // Only for Rotation/Translation Matrices
	{
		quadMatrix matrix;
		matrix._matrix[0][0] = m._matrix[0][0]; matrix._matrix[0][1] = m._matrix[1][0]; matrix._matrix[0][2] = m._matrix[2][0]; matrix._matrix[0][3] = 0.0f;
		matrix._matrix[1][0] = m._matrix[0][1]; matrix._matrix[1][1] = m._matrix[1][1]; matrix._matrix[1][2] = m._matrix[2][1]; matrix._matrix[1][3] = 0.0f;
		matrix._matrix[2][0] = m._matrix[0][2]; matrix._matrix[2][1] = m._matrix[1][2]; matrix._matrix[2][2] = m._matrix[2][2]; matrix._matrix[2][3] = 0.0f;
		matrix._matrix[3][0] = -(m._matrix[3][0] * matrix._matrix[0][0] + m._matrix[3][1] * matrix._matrix[1][0] + m._matrix[3][2] * matrix._matrix[2][0]);
		matrix._matrix[3][1] = -(m._matrix[3][0] * matrix._matrix[0][1] + m._matrix[3][1] * matrix._matrix[1][1] + m._matrix[3][2] * matrix._matrix[2][1]);
		matrix._matrix[3][2] = -(m._matrix[3][0] * matrix._matrix[0][2] + m._matrix[3][1] * matrix._matrix[1][2] + m._matrix[3][2] * matrix._matrix[2][2]);
		matrix._matrix[3][3] = 1.0f;
		return matrix;
	}

	point3D Vector_Add(point3D& v1, point3D& v2)
	{
		return { v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
	}

	point3D Vector_Sub(point3D& v1, point3D& v2)
	{
		return { v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
	}

	point3D Vector_Mul(point3D& v1, float k)
	{
		return { v1.x * k, v1.y * k, v1.z * k };
	}

	point3D Vector_Div(point3D& v1, float k)
	{
		return { v1.x / k, v1.y / k, v1.z / k };
	}

	float Vector_DotProduct(point3D& v1, point3D& v2)
	{
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}

	float Vector_Length(point3D& v)
	{
		return sqrtf(Vector_DotProduct(v, v));
	}

	point3D Vector_Normalise(point3D& v)
	{
		float l = Vector_Length(v);
		return { v.x / l, v.y / l, v.z / l };
	}

	point3D Vector_CrossProduct(point3D& v1, point3D& v2)
	{
		point3D v;
		v.x = v1.y * v2.z - v1.z * v2.y;
		v.y = v1.z * v2.x - v1.x * v2.z;
		v.z = v1.x * v2.y - v1.y * v2.x;
		return v;
	}

	point3D Vector_IntersectPlane(point3D& plane_p, point3D& plane_n, point3D& lineStart, point3D& lineEnd)
	{
		plane_n = Vector_Normalise(plane_n);
		float plane_d = -Vector_DotProduct(plane_n, plane_p);
		float ad = Vector_DotProduct(lineStart, plane_n);
		float bd = Vector_DotProduct(lineEnd, plane_n);
		float t = (-plane_d - ad) / (bd - ad);
		point3D lineStartToEnd = Vector_Sub(lineEnd, lineStart);
		point3D lineToIntersect = Vector_Mul(lineStartToEnd, t);
		return Vector_Add(lineStart, lineToIntersect);
	}

	int Triangle_ClipAgainstPlane(point3D plane_p, point3D plane_n, triPoly& in_tri, triPoly& out_tri1, triPoly& out_tri2)
	{
		// Establishing that plane normal is normal
		plane_n = Vector_Normalise(plane_n);

		// Return signed shortest distance from point to plane, plane normal must be normalised
		auto dist = [&](point3D& p)
		{
			point3D n = Vector_Normalise(p);
			return (plane_n.x * p.x + plane_n.y * p.y + plane_n.z * p.z - Vector_DotProduct(plane_n, plane_p));
		};

		/* Creating two temporary storage arrays to classify points either side of clipping plane
			If distance sign is positive, point lies on "inside" of plane */
		point3D* inside_points[3];  int nInsidePointCount = 0;
		point3D* outside_points[3]; int nOutsidePointCount = 0;

		// Getting distance of each point in triPoly to plane
		float d0 = dist(in_tri._point[0]);
		float d1 = dist(in_tri._point[1]);
		float d2 = dist(in_tri._point[2]);

		if (d0 >= 0) { inside_points[nInsidePointCount++] = &in_tri._point[0]; }
		else { outside_points[nOutsidePointCount++] = &in_tri._point[0]; }
		if (d1 >= 0) { inside_points[nInsidePointCount++] = &in_tri._point[1]; }
		else { outside_points[nOutsidePointCount++] = &in_tri._point[1]; }
		if (d2 >= 0) { inside_points[nInsidePointCount++] = &in_tri._point[2]; }
		else { outside_points[nOutsidePointCount++] = &in_tri._point[2]; }

		/* Now classify triPoly points, and break the input triPoly into
			smaller output triangles if required. There are four possible
			outcomes */

		if (nInsidePointCount == 0)
		{
			return 0; // All triangles are invalid
		}

		if (nInsidePointCount == 3)
		{
			out_tri1 = in_tri;
			return 1; // The original triPoly is valid
		}

		if (nInsidePointCount == 1 && nOutsidePointCount == 2)
		{
			if(DEBUG_MODE_STATUS)
				out_tri1._color = FG_CYAN;
			else
				out_tri1._color = in_tri._color;
			out_tri1._symbol = in_tri._symbol;
			out_tri1._point[0] = *inside_points[0];

			out_tri1._point[1] = Vector_IntersectPlane(plane_p, plane_n, *inside_points[0], *outside_points[0]);
			out_tri1._point[2] = Vector_IntersectPlane(plane_p, plane_n, *inside_points[0], *outside_points[1]);

			return 1;
		}

		if (nInsidePointCount == 2 && nOutsidePointCount == 1)
		{
			/*Making sure after clipping the quad that is formed is turned into 2 triPolys*/
			if (DEBUG_MODE_STATUS)
			{
				out_tri1._color = FG_RED;
				out_tri2._color = FG_GREEN;
			}
			else
			{
				out_tri1._color = in_tri._color;
				out_tri2._color = in_tri._color;
			}

			out_tri1._symbol = in_tri._symbol;
			out_tri2._symbol = in_tri._symbol;

			out_tri1._point[0] = *inside_points[0];
			out_tri1._point[1] = *inside_points[1];
			out_tri1._point[2] = Vector_IntersectPlane(plane_p, plane_n, *inside_points[0], *outside_points[0]);

			out_tri2._point[0] = *inside_points[1];
			out_tri2._point[1] = out_tri1._point[2];
			out_tri2._point[2] = Vector_IntersectPlane(plane_p, plane_n, *inside_points[1], *outside_points[0]);

			return 2; // Two newly formed tripolys returned
		}
	}

	// Outside resource, apologies
	CHAR_INFO GetColour(float lum)
	{
		short bg_col, fg_col;
		wchar_t sym;
		int pixel_bw = (int)(13.0f * lum);
		switch (pixel_bw)
		{
		case 0: bg_col = BG_BLACK; fg_col = FG_BLACK; sym = PIXEL_SOLID; break;

		case 1: bg_col = BG_BLACK; fg_col = FG_DARK_GREY; sym = PIXEL_QUARTER; break;
		case 2: bg_col = BG_BLACK; fg_col = FG_DARK_GREY; sym = PIXEL_HALF; break;
		case 3: bg_col = BG_BLACK; fg_col = FG_DARK_GREY; sym = PIXEL_THREEQUARTERS; break;
		case 4: bg_col = BG_BLACK; fg_col = FG_DARK_GREY; sym = PIXEL_SOLID; break;

		case 5: bg_col = BG_DARK_GREY; fg_col = FG_GREY; sym = PIXEL_QUARTER; break;
		case 6: bg_col = BG_DARK_GREY; fg_col = FG_GREY; sym = PIXEL_HALF; break;
		case 7: bg_col = BG_DARK_GREY; fg_col = FG_GREY; sym = PIXEL_THREEQUARTERS; break;
		case 8: bg_col = BG_DARK_GREY; fg_col = FG_GREY; sym = PIXEL_SOLID; break;

		case 9:  bg_col = BG_GREY; fg_col = FG_WHITE; sym = PIXEL_QUARTER; break;
		case 10: bg_col = BG_GREY; fg_col = FG_WHITE; sym = PIXEL_HALF; break;
		case 11: bg_col = BG_GREY; fg_col = FG_WHITE; sym = PIXEL_THREEQUARTERS; break;
		case 12: bg_col = BG_GREY; fg_col = FG_WHITE; sym = PIXEL_SOLID; break;
		default:
			bg_col = BG_BLACK; fg_col = FG_BLACK; sym = PIXEL_SOLID;
		}

		CHAR_INFO c;
		c.Attributes = bg_col | fg_col;
		c.Char.UnicodeChar = sym;
		return c;
	}

public:
	bool OnWindowCreate() override
	{
		// Load object file
		meshObj.LoadFromObjectFile(MODEL_NAME);

		// Projection Matrix
		matProj = Matrix_MakeProjection(90.0f, (float)ScreenHeight() / (float)ScreenWidth(), 0.1f, 1000.0f);
		return true;
	}

	bool OnWindowUpdate(float fElapsedTime) override
	{
		if (GetKey(VK_ESCAPE).bPressed)
			return false;

		if (GetKey(VK_UP).bHeld)
			vCamera.y += 8.0f * fElapsedTime;	// Travel Upwards

		if (GetKey(VK_DOWN).bHeld)
			vCamera.y -= 8.0f * fElapsedTime;	// Travel Downwards


		// TODO change to move left and right as strafe instead of in world space
		if (GetKey(VK_LEFT).bHeld)
			vCamera.x -= 8.0f * fElapsedTime;

		if (GetKey(VK_RIGHT).bHeld)
			vCamera.x += 8.0f * fElapsedTime;


		point3D vForward = Vector_Mul(vLookDir, 8.0f * fElapsedTime);

		if (GetKey(L'W').bHeld)
			vCamera = Vector_Add(vCamera, vForward);

		if (GetKey(L'S').bHeld)
			vCamera = Vector_Sub(vCamera, vForward);

		if (GetKey(L'A').bHeld)
			fYaw -= 2.0f * fElapsedTime;

		if (GetKey(L'D').bHeld)
			fYaw += 2.0f * fElapsedTime;


		quadMatrix matRotZ, matRotX;
		if(GLOBAL_SPIN_MODE_STATUS)
			fTheta += 1.0f * fElapsedTime; // Spin to debug without moving
		matRotZ = Matrix_MakeRotationZ(fTheta * 0.5f);
		matRotX = Matrix_MakeRotationX(fTheta);

		quadMatrix matTrans;
		matTrans = Matrix_MakeTranslation(0.0f, 0.0f, 5.0f);

		quadMatrix matWorld;
		matWorld = Matrix_MakeIdentity();	// World Matrix
		matWorld = Matrix_MultiplyMatrix(matRotZ, matRotX); // Transform by rotation
		matWorld = Matrix_MultiplyMatrix(matWorld, matTrans); // Transform by translation

		// "Point At" Matrix for camera
		point3D vUp = { 0,1,0 };
		point3D vTarget = { 0,0,1 };
		quadMatrix matCameraRot = Matrix_MakeRotationY(fYaw);
		vLookDir = Matrix_MultiplyVector(matCameraRot, vTarget);
		vTarget = Vector_Add(vCamera, vLookDir);
		quadMatrix matCamera = Matrix_PointAt(vCamera, vTarget, vUp);

		// view matrix from camera
		quadMatrix matView = Matrix_QuickInverse(matCamera);

		// Triangles for rastering later
		vector<triPoly> vecTrianglesToRaster;

		// Drawing Triangles
		for (auto tri : meshObj.triPolyList)
		{
			triPoly triProjected, triTransformed, triViewed;

			triTransformed._point[0] = Matrix_MultiplyVector(matWorld, tri._point[0]);
			triTransformed._point[1] = Matrix_MultiplyVector(matWorld, tri._point[1]);
			triTransformed._point[2] = Matrix_MultiplyVector(matWorld, tri._point[2]);

			// Calculate triPoly Normal
			point3D normal, line1, line2;

			// Get lines either side of triPoly
			line1 = Vector_Sub(triTransformed._point[1], triTransformed._point[0]);
			line2 = Vector_Sub(triTransformed._point[2], triTransformed._point[0]);

			// Take cross product of lines to get normal to triPoly surface
			normal = Vector_CrossProduct(line1, line2);

			// You normally need to normalise a normal!
			normal = Vector_Normalise(normal);

			// Get Ray from triPoly to camera
			point3D vCameraRay = Vector_Sub(triTransformed._point[0], vCamera);

			// If ray is aligned with normal, then triPoly is visible
			if (Vector_DotProduct(normal, vCameraRay) < 0.0f)
			{
				// Illumination TODO: Make light dynamic
				point3D light_direction = { 0.0f, 1.0f, -1.0f };
				light_direction = Vector_Normalise(light_direction);

				// How "aligned" are light direction and triPoly surface normal?
				float dp = max(0.1f, Vector_DotProduct(light_direction, normal));

				// Choosing console colours as required (much easier with RGB)
				CHAR_INFO c = GetColour(dp);
				triTransformed._color = c.Attributes;
				triTransformed._symbol = c.Char.UnicodeChar;

				// Convert World Space --> View Space
				triViewed._point[0] = Matrix_MultiplyVector(matView, triTransformed._point[0]);
				triViewed._point[1] = Matrix_MultiplyVector(matView, triTransformed._point[1]);
				triViewed._point[2] = Matrix_MultiplyVector(matView, triTransformed._point[2]);
				triViewed._symbol = triTransformed._symbol;
				triViewed._color = triTransformed._color;

				// Clipping Viewed Triangle against near plane, this could form two additional
				// additional triangles. 
				int nClippedTriangles = 0;
				triPoly clipped[2];
				nClippedTriangles = Triangle_ClipAgainstPlane({ 0.0f, 0.0f, 0.1f }, { 0.0f, 0.0f, 1.0f }, triViewed, clipped[0], clipped[1]);

				for (int n = 0; n < nClippedTriangles; n++)
				{
					// Project triangles from 3D --> 2D
					triProjected._point[0] = Matrix_MultiplyVector(matProj, clipped[n]._point[0]);
					triProjected._point[1] = Matrix_MultiplyVector(matProj, clipped[n]._point[1]);
					triProjected._point[2] = Matrix_MultiplyVector(matProj, clipped[n]._point[2]);
					triProjected._color = clipped[n]._color;
					triProjected._symbol = clipped[n]._symbol;

					triProjected._point[0] = Vector_Div(triProjected._point[0], triProjected._point[0].w);
					triProjected._point[1] = Vector_Div(triProjected._point[1], triProjected._point[1].w);
					triProjected._point[2] = Vector_Div(triProjected._point[2], triProjected._point[2].w);

					// Reverting inverted X/Y
					triProjected._point[0].x *= -1.0f;
					triProjected._point[1].x *= -1.0f;
					triProjected._point[2].x *= -1.0f;
					triProjected._point[0].y *= -1.0f;
					triProjected._point[1].y *= -1.0f;
					triProjected._point[2].y *= -1.0f;

					// Offset verts into visible normalised space
					point3D vOffsetView = { 1,1,0 };
					triProjected._point[0] = Vector_Add(triProjected._point[0], vOffsetView);
					triProjected._point[1] = Vector_Add(triProjected._point[1], vOffsetView);
					triProjected._point[2] = Vector_Add(triProjected._point[2], vOffsetView);
					triProjected._point[0].x *= 0.5f * (float)ScreenWidth();
					triProjected._point[0].y *= 0.5f * (float)ScreenHeight();
					triProjected._point[1].x *= 0.5f * (float)ScreenWidth();
					triProjected._point[1].y *= 0.5f * (float)ScreenHeight();
					triProjected._point[2].x *= 0.5f * (float)ScreenWidth();
					triProjected._point[2].y *= 0.5f * (float)ScreenHeight();

					// Store triPoly for sorting
					vecTrianglesToRaster.push_back(triProjected);
				}
			}
		}

		// Sort triangles from back to front
		sort(vecTrianglesToRaster.begin(), vecTrianglesToRaster.end(), [](triPoly& t1, triPoly& t2)
			{
				float z1 = (t1._point[0].z + t1._point[1].z + t1._point[2].z) / 3.0f;
				float z2 = (t2._point[0].z + t2._point[1].z + t2._point[2].z) / 3.0f;
				return z1 > z2;
			});

		// Clear Screen
		Fill(0, 0, ScreenWidth(), ScreenHeight(), PIXEL_SOLID, FG_BLACK);

		// Loop through all transformed, viewed, projected, and sorted triangles
		for (auto& triToRaster : vecTrianglesToRaster)
		{
			// Clip triangles against all four screen edges, this could yield
			// a bunch of triangles, so create a queue that we traverse to 
			//  ensure we only test new triangles generated against planes
			triPoly clipped[2];
			list<triPoly> listTriangles;

			// Add initial triPoly
			listTriangles.push_back(triToRaster);
			int nNewTriangles = 1;

			for (int p = 0; p < 4; p++)
			{
				int nTrisToAdd = 0;
				while (nNewTriangles > 0)
				{
					// Take triPoly from front of queue
					triPoly test = listTriangles.front();
					listTriangles.pop_front();
					nNewTriangles--;

					// Clipping it against a plane
					switch (p)
					{
					case 0:	nTrisToAdd = Triangle_ClipAgainstPlane({ 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, test, clipped[0], clipped[1]); break;
					case 1:	nTrisToAdd = Triangle_ClipAgainstPlane({ 0.0f, (float)ScreenHeight() - 1, 0.0f }, { 0.0f, -1.0f, 0.0f }, test, clipped[0], clipped[1]); break;
					case 2:	nTrisToAdd = Triangle_ClipAgainstPlane({ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, test, clipped[0], clipped[1]); break;
					case 3:	nTrisToAdd = Triangle_ClipAgainstPlane({ (float)ScreenWidth() - 1, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, test, clipped[0], clipped[1]); break;
					}

					// Clipping may yield a variable number of triangles, adding back to queue
					for (int w = 0; w < nTrisToAdd; w++)
						listTriangles.push_back(clipped[w]);
				}
				nNewTriangles = listTriangles.size();
			}


			// Draw the transformed, viewed, clipped, projected, sorted, clipped triangles
			for (auto& t : listTriangles)
			{
				FillTriangle(t._point[0].x, t._point[0].y, t._point[1].x, t._point[1].y, t._point[2].x, t._point[2].y, t._symbol, t._color);
				if (DEBUG_MODE_STATUS)
				{
					DrawTriangle(t._point[0].x, t._point[0].y, t._point[1].x, t._point[1].y, t._point[2].x, t._point[2].y, PIXEL_SOLID, FG_BLACK);
				}
			}
		}


		return true;
	}

};




int main()
{
	int __consoleWidth = 140, __consoleHeight = 80, tmp;
	char debugTmp = 'N';
	cout << "Input Console Width (Min: 140 please): ";
	cin >> tmp;
	if (tmp >= 140)
	{
		__consoleWidth = tmp;
	}
	cout << "Input Console Height (Min: 80 please): ";
	cin >> tmp;
	if (tmp >= 80)
	{
		__consoleHeight = tmp;
	}
	cout << "Turn debug mode ON? (Y/N)" << endl;
	cin >> debugTmp;
	if (debugTmp == 'Y')
	{
		DEBUG_MODE_STATUS = true;
	}
	else
	{
		DEBUG_MODE_STATUS = false;
	}

	int modelIdx = 0;
	vector<string> modelNameList = { "terrain.obj", "teapot.obj", "axis3d.obj", "spaceship.obj"};
	cout << "Select a model to load:\n\n1. Terrain\n2. Teapot\n3. 3D Axis\n4. Space Ship (Basic)\n\nInput the number referencing model: ";
	cin >> tmp;
	if (tmp > 0 && tmp < 5)
	{
		modelIdx = tmp - 1;
	}
	MODEL_NAME = modelNameList[modelIdx];
	cout << "Enable Global World Spin? (Y/N)" << endl;
	cin >> debugTmp;
	if (debugTmp == 'Y')
	{
		GLOBAL_SPIN_MODE_STATUS = true;
	}
	else
	{
		GLOBAL_SPIN_MODE_STATUS = false;
	}
	consoleEngine3D gameDemo;
	if (gameDemo.ConstructConsole(__consoleWidth, __consoleHeight, 1, 1))
		gameDemo.Start();
	return 0;
}

