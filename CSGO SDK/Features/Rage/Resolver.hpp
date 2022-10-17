#pragma once
#include "LagCompensation.hpp"
#include <vector>
#include <deque>

namespace Engine {
	// taken from supremacy
	enum EResolverModes : size_t {
		RESOLVE_NONE = 0,
		RESOLVE_WALK,
		RESOLVE_STAND,
		RESOLVE_FREESTAND,
		RESOLVE_LAST_LBY,
		RESOLVE_DISTORT,
		RESOLVE_BRUTEFORCE,
		RESOLVE_AIR,
		RESOLVE_LBY_UPDATE,
		RESOLVE_STOPPED_MOVING,
	};

	struct CResolverData {
		struct LastMoveData_t {
			float m_flLowerBodyYawTarget;
			float m_flSimulationTime;
			float m_flAnimTime;
			Vector m_vecOrigin;
		};

		bool m_bCollectedValidMoveData;
		bool m_bWentDormant;
		bool m_bPredictingUpdates;

		Vector m_vecSavedOrigin;
		LastMoveData_t m_sMoveData;

		float m_flBestYaw;
		float m_flBestDistance;

		float m_flNextBodyUpdate;
		float m_flFinalResolverYaw;
		float m_flOldLowerBodyYawTarget;
		bool  m_bCollectedFreestandData;
		int m_iMissedShots = 0;
		int m_iMissedShotsLBY = 0;

	};

	class CResolver {
	private:
		void ResolveAngles(C_CSPlayer* player, C_AnimationRecord* record);
		void ResolveWalk(C_CSPlayer* player, C_AnimationRecord* record);
		void ResolveStand(C_CSPlayer* player, C_AnimationRecord* record);
		void ResolveAir(C_CSPlayer* player, C_AnimationRecord* record);
		bool ShouldUseFreestand(C_CSPlayer* player, C_AnimationRecord* record);
		void Freestand(C_CSPlayer* player, C_AnimationRecord* record);
		int GetChokedPackets(C_CSPlayer* player);
		bool bFacingright;
		bool bFacingleft;
	public:
		void ResolveManual(C_CSPlayer* player, C_AnimationRecord* record, bool bDisallow = false);
		void ResolveYaw(C_CSPlayer* player, C_AnimationRecord* record);

	public:
		// check if the players yaw is sideways.
		bool IsLastMoveValid(C_AnimationRecord* record, float m_yaw) {
			auto local = C_CSPlayer::GetLocalPlayer();
			if (!local)
				return false;
			Vector angAway;
			Math::VectorAngles(local->m_vecOrigin() - record->m_vecOrigin, angAway);
			const float delta = fabs(Math::AngleNormalize(angAway.y - m_yaw));
			return delta > 20.f && delta < 160.f;
		}

		// freestanding.
		class AdaptiveAngle {
		public:
			float m_yaw;
			float m_dist;

		public:
			// ctor.
			__forceinline AdaptiveAngle() :
				m_yaw{ },
				m_dist{ }
			{ };

			__forceinline AdaptiveAngle(float yaw, float penalty = 0.f) {
				// set yaw.
				m_yaw = Math::AngleNormalize(yaw);

				// init distance.
				m_dist = 0.f;

				// remove penalty.
				m_dist -= penalty;
			}
		};
		void FindBestAngle(C_CSPlayer* player);
	};

	extern CResolver g_Resolver;
	extern CResolverData g_ResolverData[65];
}