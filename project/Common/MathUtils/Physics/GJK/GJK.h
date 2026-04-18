#pragma once
#include "Collider.h"
#include <array>

namespace GJK {
	namespace _3D{

		Vector3 Support(const Collider& colliderA, const Collider& colliderB, Vector3 direction);

		struct Simplex {
		private:
			std::array<Vector3, 4> m_points;
			int m_size;

		public:
			Simplex()
				: m_size(0)
			{
			}

			Simplex& operator=(std::initializer_list<Vector3> list)
			{
				m_size = 0;

				for (Vector3 point : list)
					m_points[m_size++] = point;

				return *this;
			}

			void push_front(Vector3 point)
			{
				m_points = { point, m_points[0], m_points[1], m_points[2] };
				m_size = (std::min)(m_size + 1, 4);
			}

			Vector3& operator[](int i) { return m_points[i]; }
			size_t size() const { return m_size; }

			auto begin() const { return m_points.begin(); }
			auto end() const { return m_points.end() - (4 - m_size); }
		};

		bool SameDirection(const Vector3& direction, const Vector3& ao);

		bool Line(Simplex& points, Vector3& direction);

		bool Triangle(Simplex& points, Vector3& direction);

		bool Tetrahedron(Simplex& points, Vector3& direction);

		bool NextSimplex(Simplex& points, Vector3& direction);

		struct FaceNormal {
			Vector3 normal;
			float distance;
			FaceNormal() : normal{ 0,0,0 }, distance(0.0f) {}
			FaceNormal(const Vector3& n, float d) : normal(n), distance(d) {}
		};

		std::pair<std::vector<FaceNormal>, size_t> GetFaceNormals(
			const std::vector<Vector3>& polytope,
			const std::vector<size_t>& faces);

		void AddIfUniqueEdge(
			std::vector<std::pair<size_t, size_t>>& edges,
			const std::vector<size_t>& faces,
			size_t a, size_t b);

		std::pair<size_t, size_t> AddSupportToPolytope(
			std::vector<Vector3>& polytope,
			std::vector<size_t>& faces,
			const Vector3& support);

		struct Contact3D {
			Vector3 normal;
			float depth;
			bool hasCollision;
			Contact3D() : normal{ 0,0,0 }, depth(0.0f), hasCollision(false) {}
			Contact3D(const Vector3& n, float d) : normal(n), depth(d), hasCollision(true) {}
		};

		bool GJK_GetSimplex(const Collider& colliderA, const Collider& colliderB, std::vector<Vector3>& outSimplex);

		Contact3D EPA(const Collider& colliderA, const Collider& colliderB, const std::vector<Vector3>& initialSimplex);
	}
}