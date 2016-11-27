/*
 * Platformer Game Engine by Wohlstand, a free platform for game making
 * Copyright (c) 2016 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../scene_level.h"
#include <cmath>

int LevelScene::findNearestSection(long x, long y)
{
    long lessDistance = 0;
    int  result = 0;

    //Find section by nearest center or corner
    for(int i = 0; i < data.sections.size(); i++)
    {
        LevelSection &s = data.sections[i];
        long centerX = s.size_left + std::abs(s.size_left - s.size_right) / 2;
        long centerY = s.size_top  + std::abs(s.size_top  - s.size_bottom) / 2;
        long distanceC = std::sqrt(std::pow(centerX - x, 2) + std::pow(centerY - y, 2));
        long distanceCorner = distanceC;
        distanceCorner = std::sqrt(std::pow(s.size_left - x, 2) + std::pow(s.size_top - y, 2));

        if(distanceC > distanceCorner)
            distanceC = distanceCorner;

        distanceCorner = std::sqrt(std::pow(s.size_right - x, 2) + std::pow(s.size_top - y, 2));

        if(distanceC > distanceCorner)
            distanceC = distanceCorner;

        distanceCorner = std::sqrt(std::pow(s.size_right - x, 2) + std::pow(s.size_bottom - y, 2));

        if(distanceC > distanceCorner)
            distanceC = distanceCorner;

        distanceCorner = std::sqrt(std::pow(s.size_left - x, 2) + std::pow(s.size_bottom - y, 2));

        if(distanceC > distanceCorner)
            distanceC = distanceCorner;

        if(i == 0)
            lessDistance = distanceC;
        else if(distanceC < lessDistance)
        {
            lessDistance = distanceC;
            result = i;
        }
    }

    return result;
}



LVL_Section *LevelScene::getSection(int sct)
{
    if((sct >= 0) && (sct < sections.size()))
    {
        if(sections[sct].data.id == sct)
            return &sections[sct];
        else
        {
            for(LVL_SectionsList::iterator it = sections.begin(); it != sections.end(); it++)
                if(it->data.id == sct)
                    return &(*it);
        }
    }

    return nullptr;
}

EpisodeState *LevelScene::getGameState()
{
    return gameState;
}


