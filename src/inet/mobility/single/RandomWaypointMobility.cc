//
// Copyright (C) 2005 Georg Lutz, Institut fuer Telematik, University of Karlsruhe
// Copyright (C) 2005 OpenSim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//

#include "inet/mobility/single/RandomWaypointMobility.h"

namespace inet {

Define_Module(RandomWaypointMobility);

RandomWaypointMobility::RandomWaypointMobility()
{
    nextMoveIsWait = false;
    borderPolicy = RAISEERROR;
}

void RandomWaypointMobility::initialize(int stage)
{
    LineSegmentsMobilityBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        borderPolicy = RAISEERROR;
        waitTimeParameter = &par("waitTime");
        hasWaitTime = waitTimeParameter->isExpression() || waitTimeParameter->doubleValue() != 0;
        speedParameter = &par("speed");
        stationary = !speedParameter->isExpression() && speedParameter->doubleValue() == 0;
    }
}

void RandomWaypointMobility::setTargetPosition()
{
    if (nextMoveIsWait) {
        simtime_t waitTime = waitTimeParameter->doubleValue();
        targetPosition = segmentStartPosition = lastPosition;
        segmentStartOrientation = lastOrientation;
        segmentStartVelocity = Coord::ZERO;
        nextChange = simTime() + waitTime;
        nextMoveIsWait = false;
    }
    else {
        segmentStartPosition = lastPosition;
        targetPosition = getRandomPosition();
        double speed = speedParameter->doubleValue();
        double distance = segmentStartPosition.distance(targetPosition);
        simtime_t travelTime = distance / speed;
        segmentStartVelocity = (targetPosition - segmentStartPosition).normalize() * speed;
        segmentStartOrientation = faceForward ? getOrientOfVelocity(segmentStartVelocity) : lastOrientation;

        nextChange = simTime() + travelTime;
        nextMoveIsWait = hasWaitTime;
    }
}

double RandomWaypointMobility::getMaxSpeed() const
{
    return speedParameter->isExpression() ? NaN : speedParameter->doubleValue();
}

} // namespace inet

