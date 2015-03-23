/*
Copyright (C) 2010 Srivats P.

This file is part of "Ostinato"

This is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#include "portgrouplist.h"

// TODO(LOW): Remove
#include <modeltest.h>

PortGroupList::PortGroupList()
    : mPortGroupListModel(this), 
      mStreamListModel(this),
      mPortStatsModel(this, this)
{
    PortGroup    *pg;

#ifdef QT_NO_DEBUG
    streamModelTester_ = NULL;
    portModelTester_ = NULL;
    portStatsModelTester_ = NULL;
#else
    streamModelTester_ = new ModelTest(getStreamModel());
    portModelTester_ = new ModelTest(getPortModel());
    portStatsModelTester_ = new ModelTest(getPortStatsModel());
#endif 

    // Add the "Local" Port Group
    pg = new PortGroup;
    addPortGroup(*pg);
}

PortGroupList::~PortGroupList()
{
    delete portStatsModelTester_;
    delete portModelTester_;
    delete streamModelTester_;

    while (!mPortGroups.isEmpty())
        delete mPortGroups.takeFirst();

}

bool PortGroupList::isPortGroup(const QModelIndex& index)
{
    return mPortGroupListModel.isPortGroup(index);
}

bool PortGroupList::isPort(const QModelIndex& index)
{
    return mPortGroupListModel.isPort(index);
}

PortGroup& PortGroupList::portGroup(const QModelIndex& index)
{
    Q_ASSERT(mPortGroupListModel.isPortGroup(index));

    return *(mPortGroups[index.row()]);
}

Port& PortGroupList::port(const QModelIndex& index)
{
    Q_ASSERT(mPortGroupListModel.isPort(index));
    return (*mPortGroups.at(index.parent().row())->mPorts[index.row()]);
}

void PortGroupList::addPortGroup(PortGroup &portGroup)
{
    mPortGroupListModel.portGroupAboutToBeAppended();

    connect(&portGroup, SIGNAL(portGroupDataChanged(int, int)),
        &mPortGroupListModel, SLOT(when_portGroupDataChanged(int, int)));
#if 0
    connect(&portGroup, SIGNAL(portListAboutToBeChanged(quint32)),
        &mPortGroupListModel, SLOT(triggerLayoutAboutToBeChanged()));
    connect(&portGroup, SIGNAL(portListChanged(quint32)),
        &mPortGroupListModel, SLOT(triggerLayoutChanged()));
#endif
    connect(&portGroup, SIGNAL(portListChanged(quint32)),
        &mPortGroupListModel, SLOT(when_portListChanged()));

    connect(&portGroup, SIGNAL(portListChanged(quint32)),
        &mPortStatsModel, SLOT(when_portListChanged()));

    connect(&portGroup, SIGNAL(statsChanged(quint32)),
        &mPortStatsModel, SLOT(when_portGroup_stats_update(quint32)));

    mPortGroups.append(&portGroup);
    portGroup.connectToHost();

    mPortGroupListModel.portGroupAppended();

    mPortStatsModel.when_portListChanged();
}

void PortGroupList::removePortGroup(PortGroup &portGroup)
{
    mPortGroupListModel.portGroupAboutToBeRemoved(&portGroup);

    PortGroup* pg = mPortGroups.takeAt(mPortGroups.indexOf(&portGroup));
    qDebug("after takeAt()");
    mPortGroupListModel.portGroupRemoved();

    delete pg;

    mPortStatsModel.when_portListChanged();
}

//....................
// Private Methods
//....................
int PortGroupList::indexOfPortGroup(quint32 portGroupId)
{
    for (int i = 0; i < mPortGroups.size(); i++) {
        if (mPortGroups.value(i)->id() == portGroupId)
            return i;
    } 
    return -1;
}
