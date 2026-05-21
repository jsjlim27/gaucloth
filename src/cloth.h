#pragma once
/*
 * cloth.h -- the cloth simulation state and solver.
 *
 * IMPORTANT: this file contains NO OpenGL. The physics knows nothing about
 * rendering. It owns positions/velocities/masses and advances them in time.
 * The renderer (in main.cpp) READS positions from here each frame and uploads
 * them to the GPU. That one-way handoff is the whole architecture.
 *
 * As of now: gravity + explicit Euler integration only. No constraints yet --
 * so the cloth will fall apart. That is intentional; later will add the
 * stretch constraints that hold it together.
 */

#include <glm/glm.hpp>
#include <vector>

struct Cloth {
	// --- simulation state (separate arrays, indexed in parallel) ---
	std::vector<glm::vec3> positions;  // current position of each particle
	std::vector<glm::vec3> velocities; // current velocity of each particle
	std::vector<float>     inv_mass;   // 1/mass; 0 means PINNED (infinite mass)

	// --- a distance (stretch) constraint between two particles ---
	struct Constraint {
		int a, b; 	   // particle indices
		float rest_length; // L0
		float lambda;      // accumulated Lagrange multiplier (reset each substep)
	};
	std::vector<Constraint> constraints;

	int N = 0; // grid is N x N
	float spacing = 0.0f;

	glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);

	// --- tunable solver parameters ---
	float compliance = 1.0e-6f; // 1/stiffness. small = stiff, large = stretchy
	int substeps = 20;          // physics substeps per frame (stability + stiffness)
	int solver_iters = 5;       // constraint passes per substep

	int index(int i, int j) const
	{
		return i * N + j;
	}

	// --------------------------------------------------------------------
	// Build a flat N x N grid in the XZ plane, centered at the origin,
	// and create structural stretch constraints between neighbors.
	// --------------------------------------------------------------------
	void init_grid(int n, float space) 
	{
		N = n;
		spacing = space;
		const int count = N * N;

		positions.assign(count, glm::vec3(0.0f));
		velocities.assign(count, glm::vec3(0.0f));
		inv_mass.assign(count, 1.0f);
		constraints.clear();

		const float half = (N - 1) * spacing * 0.5f;
		for (int i = 0; i < N; ++i)
		{
			for (int j = 0; j < N; ++j)
			{
				positions[index(i, j)] = 
					glm::vec3(j * spacing - half,
						  0.0f,
						  i * spacing - half);
			}
		}

		// structural constraints: connect each particle to its right and 
		// down neighbors (covers every horizontal and vertical edge once).
		for (int i = 0; i < N; ++i)
		{
			for (int j = 0; j < N; ++j)
			{
				if (j + 1 < N)
				{
					// horizontal
					add_constraint(index(i, j), index(i, j + 1));
				}
				if (i + 1 < N)
				{
					// vertical
					add_constraint(index(i, j), index(i + 1, j));
				}
			}
		}
	}

	void add_constraint(int a, int b)
	{
		float L0 = glm::length(positions[a] - positions[b]);
		constraints.push_back({a, b, L0, 0.0f});
	}


	// Pin a particle: inv_mass = 0 makes it immovable (infinite mass).
	void pin(int index)
	{
		if (index >= 0 && index < (int)inv_mass.size())
		{
			inv_mass[index] = 0.0f;
		}
	}

	// --------------------------------------------------------------------
	// Triangle connectivity for rendering the grid as as surface.
	// --------------------------------------------------------------------
	std::vector<unsigned int> build_indices() const
	{
		std::vector<unsigned int> idx;
		idx.reserve((N - 1) * (N - 1) * 6); // 2 triangles * 3 verts per cell
		for (int i = 0; i < N - 1; ++i)
		{
			for (int j = 0; j < N - 1; ++j)
			{
				unsigned int tl = index(i, j);
				unsigned int tr = index(i, j + 1);
				unsigned int bl = index(i + 1, j);
				unsigned int br = index(i + 1, j + 1);

				// triangle 1
				idx.push_back(tl);
				idx.push_back(bl);
				idx.push_back(tr);

				// triangle 2
				idx.push_back(tr);
				idx.push_back(bl);
				idx.push_back(br);
			}
		}
		return idx;
	}

	// --------------------------------------------------------------------
	// Solve one distance constraint (the core XPBD update).
	//   C = |pa - pb| - L0
	//   n = (pa - pb) / |pa - pb|
	//   a~ = compliance / dt^2
	//   dl = (-C - a~ * lambda) / (wa + wb + a~)
	//   pa += wa * dl * n ; pb -= wb * dl * n
	// --------------------------------------------------------------------
	void solve_constraint(Constraint& c, float dt)
	{
		const float wa = inv_mass[c.a];
		const float wb = inv_mass[c.b];
		const float w_sum = wa + wb;
		if (w_sum == 0.0f)
		{
			return; // both pinned: nothing to do
		}

		glm::vec3 d = positions[c.a] - positions[c.b];
		float len = glm::length(d);
		if (len < 1e-9f)
		{
			return; // avoid divide-by-zero on coincident points
		}

		glm::vec3 n = d / len;
		float C = len - c.rest_length;

		float alpha = compliance / (dt * dt);
		float dlambda = (-C - alpha * c.lambda) / (w_sum + alpha);
		c.lambda += dlambda;

		glm::vec3 corr = dlambda * n;
		positions[c.a] += wa * corr;
		positions[c.b] -= wb * corr;
	}

	// --------------------------------------------------------------------
	// Advance the simulation by dt seconds using substepped XPBD.
	// --------------------------------------------------------------------
	void step(float dt)
	{
		const int count = (int)positions.size();
		const float h = dt / (float)substeps; // substep timestep

		for (int s = 0; s < substeps; ++s)
		{
			// 1. predict: save old positions, integrate gravity
			//    (we reuse velocities; old position is recovered as we go)
			static thread_local std::vector<glm::vec3> prev;
			prev.resize(count);

			for (int p = 0; p < count; ++p)
			{
				prev[p] = positions[p];
				if (inv_mass[p] == 0.0f) continue; // pinned: don't move
								   //
				// explicit Euler: v += g*dt ; x += v*dt
				velocities[p] += gravity * h;
				positions[p] += velocities[p] * h;
			}

			// 2. reset Lagrange multipliers for this substep
			for (auto& c : constraints) c.lambda = 0.0f;

			// 3. solve constraints (one or more passes)
			for (int it = 0; it < solver_iters; ++it)
			{
				for (auto& c : constraints)
				{
					solve_constraint(c, h);
				}
			}

			// 4. derive velocities from the actual position change
			for (int p = 0; p < count; ++p)
			{
				if (inv_mass[p] == 0.0f)
				{
					velocities[p] = glm::vec3(0.0f);
					continue;
				}
				velocities[p] = (positions[p] - prev[p]) / h;
			}
		}
	}
};
