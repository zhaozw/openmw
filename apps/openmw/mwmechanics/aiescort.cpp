#include "aiescort.hpp"

#include "../mwbase/world.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"

#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/timestamp.hpp"

#include "steering.hpp"
#include "movement.hpp"

namespace
{
    float sgn(float a)
    {
        if(a > 0)
            return 1.0;
        return -1.0;
    }
}

/*
    TODO: Test vanilla behavior on passing x0, y0, and z0 with duration of anything including 0.
    TODO: Different behavior for AIEscort a d x y z and AIEscortCell a c d x y z.
    TODO: Take account for actors being in different cells.
*/

namespace MWMechanics
{
    AiEscort::AiEscort(const std::string &actorId, int duration, float x, float y, float z)
    : mActorId(actorId), mX(x), mY(y), mZ(z), mDuration(duration)
    , mCellX(std::numeric_limits<int>::max())
    , mCellY(std::numeric_limits<int>::max())
    {
        mMaxDist = 470;

        // The CS Help File states that if a duration is given, the AI package will run for that long
        // BUT if a location is givin, it "trumps" the duration so it will simply escort to that location.
        if(mX != 0 || mY != 0 || mZ != 0)
            mDuration = 0;

        else
        {
            MWWorld::TimeStamp startTime = MWBase::Environment::get().getWorld()->getTimeStamp();
            mStartingSecond = ((startTime.getHour() - int(startTime.getHour())) * 100);
        }
    }

    AiEscort::AiEscort(const std::string &actorId, const std::string &cellId,int duration, float x, float y, float z)
    : mActorId(actorId), mCellId(cellId), mX(x), mY(y), mZ(z), mDuration(duration)
    , mCellX(std::numeric_limits<int>::max())
    , mCellY(std::numeric_limits<int>::max())
    {
        mMaxDist = 470;

        // The CS Help File states that if a duration is given, the AI package will run for that long
        // BUT if a location is givin, it "trumps" the duration so it will simply escort to that location.
        if(mX != 0 || mY != 0 || mZ != 0)
            mDuration = 0;

        else
        {
            MWWorld::TimeStamp startTime = MWBase::Environment::get().getWorld()->getTimeStamp();
            mStartingSecond = ((startTime.getHour() - int(startTime.getHour())) * 100);
        }
    }


    AiEscort *MWMechanics::AiEscort::clone() const
    {
        return new AiEscort(*this);
    }

    bool AiEscort::execute (const MWWorld::Ptr& actor,float duration)
    {
        // If AiEscort has ran for as long or longer then the duration specified
        // and the duration is not infinite, the package is complete.
        if(mDuration != 0)
        {
            MWWorld::TimeStamp current = MWBase::Environment::get().getWorld()->getTimeStamp();
            unsigned int currentSecond = ((current.getHour() - int(current.getHour())) * 100);
            if(currentSecond - mStartingSecond >= mDuration)
                return true;
        }

        MWWorld::Ptr player = MWBase::Environment::get().getWorld()->getPlayerPtr();
        ESM::Position pos = actor.getRefData().getPosition();
        bool cellChange = actor.getCell()->getCell()->mData.mX != mCellX || actor.getCell()->getCell()->mData.mY != mCellY;

        if(actor.getCell()->getCell()->mData.mX != player.getCell()->getCell()->mData.mX)
        {
            int sideX = sgn(actor.getCell()->getCell()->mData.mX - player.getCell()->getCell()->mData.mX);
            // Check if actor is near the border of an inactive cell. If so, pause walking.
            if(sideX * (pos.pos[0] - actor.getCell()->getCell()->mData.mX * ESM::Land::REAL_SIZE) > sideX * (ESM::Land::REAL_SIZE /
                2.0 - 200))
            {
                MWWorld::Class::get(actor).getMovementSettings(actor).mPosition[1] = 0;
                return false;
            }
        }
        if(actor.getCell()->getCell()->mData.mY != player.getCell()->getCell()->mData.mY)
        {
            int sideY = sgn(actor.getCell()->getCell()->mData.mY - player.getCell()->getCell()->mData.mY);
            // Check if actor is near the border of an inactive cell. If so, pause walking.
            if(sideY*(pos.pos[1] - actor.getCell()->getCell()->mData.mY * ESM::Land::REAL_SIZE) > sideY * (ESM::Land::REAL_SIZE /
                2.0 - 200))
            {
                MWWorld::Class::get(actor).getMovementSettings(actor).mPosition[1] = 0;
                return false;
            }
        }


        if(!mPathFinder.isPathConstructed() || cellChange)
        {
            mCellX = actor.getCell()->getCell()->mData.mX;
            mCellY = actor.getCell()->getCell()->mData.mY;

            ESM::Pathgrid::Point dest;
            dest.mX = mX;
            dest.mY = mY;
            dest.mZ = mZ;

            ESM::Pathgrid::Point start;
            start.mX = pos.pos[0];
            start.mY = pos.pos[1];
            start.mZ = pos.pos[2];

            mPathFinder.buildPath(start, dest, actor.getCell(), true);
        }

        if(mPathFinder.checkPathCompleted(pos.pos[0],pos.pos[1],pos.pos[2]))
        {
            MWWorld::Class::get(actor).getMovementSettings(actor).mPosition[1] = 0;
            return true;
        }

        const MWWorld::Ptr follower = MWBase::Environment::get().getWorld()->getPtr(mActorId, false);
        const float* const leaderPos = actor.getRefData().getPosition().pos;
        const float* const followerPos = follower.getRefData().getPosition().pos;
        double differenceBetween[3];

        for (short counter = 0; counter < 3; counter++)
            differenceBetween[counter] = (leaderPos[counter] - followerPos[counter]);

        float distanceBetweenResult =
            (differenceBetween[0] * differenceBetween[0]) + (differenceBetween[1] * differenceBetween[1]) + (differenceBetween[2] *
                differenceBetween[2]);

        if(distanceBetweenResult <= mMaxDist * mMaxDist)
        {
            float zAngle = mPathFinder.getZAngleToNext(pos.pos[0], pos.pos[1]);
            zTurn(actor, Ogre::Degree(zAngle));
            MWWorld::Class::get(actor).getMovementSettings(actor).mPosition[1] = 1;
            mMaxDist = 470;
        }
        else
        {
            // Stop moving if the player is to far away
            MWBase::Environment::get().getMechanicsManager()->playAnimationGroup(actor, "idle3", 0, 1);
            MWWorld::Class::get(actor).getMovementSettings(actor).mPosition[1] = 0;
            mMaxDist = 330;
        }

        return false;
    }

    int AiEscort::getTypeId() const
    {
        return TypeIdEscort;
    }
}

