/*
  Q Light Controller Plus
  functionstreewidget.cpp

  Copyright (c) Massimo Callegari

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  Version 2 as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details. The license is
  in the file "COPYING".

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <QDebug>

#include "functionstreewidget.h"
#include "function.h"
#include "chaser.h"
#include "scene.h"
#include "doc.h"
#include <QContextMenuEvent>

#define COL_NAME 0
#define COL_PATH 1

FunctionsTreeWidget::FunctionsTreeWidget(Doc *doc, QWidget *parent) :
    QTreeWidget(parent)
  , m_doc(doc)
  , m_draggedItem(NULL)
{
    sortItems(COL_NAME, Qt::AscendingOrder);

    QTreeWidgetItem *root = invisibleRootItem();
    root->setFlags(root->flags() & ~Qt::ItemIsDropEnabled);

    connect(this, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
                this, SLOT(slotItemChanged(QTreeWidgetItem*)));
}

void FunctionsTreeWidget::updateTree()
{
    blockSignals(true);

    clearTree();

    foreach (Function* function, m_doc->functions())
        updateFunctionItem(new QTreeWidgetItem(parentItem(function)), function);

    blockSignals(false);
}

void FunctionsTreeWidget::clearTree()
{
    m_foldersMap.clear();
    clear();
}

void FunctionsTreeWidget::functionChanged(quint32 fid)
{
    blockSignals(true);
    Function* function = m_doc->function(fid);
    if (function == NULL)
        return;

    QTreeWidgetItem* item = functionItem(function);
    if (item != NULL)
        updateFunctionItem(item, function);
    blockSignals(false);
}

void FunctionsTreeWidget::functionAdded(quint32 fid)
{
    blockSignals(true);
    Function* function = m_doc->function(fid);
    if (function == NULL)
        return;

    QTreeWidgetItem* item = new QTreeWidgetItem(parentItem(function));
    updateFunctionItem(item, function);
    blockSignals(false);
}

void FunctionsTreeWidget::updateFunctionItem(QTreeWidgetItem* item, const Function* function)
{
    Q_ASSERT(item != NULL);
    Q_ASSERT(function != NULL);
    item->setText(COL_NAME, function->name());
    item->setIcon(COL_NAME, functionIcon(function));
    item->setData(COL_NAME, Qt::UserRole, function->id());
    item->setData(COL_NAME, Qt::UserRole + 1, function->type());
    item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);
}

QTreeWidgetItem* FunctionsTreeWidget::parentItem(const Function* function)
{
    Q_ASSERT(function != NULL);

    // Special case for Sequences. They belong to a Scene node
    if (function->type() == Function::Chaser && qobject_cast<const Chaser*>(function)->isSequence() == true)
    {
        quint32 sid = qobject_cast<const Chaser*>(function)->getBoundSceneID();

        QTreeWidgetItem *sceneTopItem = folderItem("Scene/");
        if (sceneTopItem != NULL)
        {
            for (int i = 0; i < sceneTopItem->childCount(); i++)
            {
                QTreeWidgetItem *child = sceneTopItem->child(i);
                Q_ASSERT(child != NULL);

                if (sid == itemFunctionId(child))
                    return child;
            }
        }
    }

    QTreeWidgetItem *pItem = folderItem(function->path());

    if (pItem != NULL)
    {
        qDebug() << "Found item for function:" << function->name() << ", path: " << function->path();
        return pItem;
    }

    // Parent item for the given type doesn't exist yet so create one
    QTreeWidgetItem* item = new QTreeWidgetItem(this);
    item->setText(COL_NAME, Function::typeToString(function->type()));
    item->setIcon(COL_NAME, functionIcon(function));
    item->setData(COL_NAME, Qt::UserRole, Function::invalidId());
    item->setData(COL_NAME, Qt::UserRole + 1, function->type());
    item->setText(COL_PATH, function->path());
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled);
    m_foldersMap[function->path()] = item;
    return item;
}

quint32 FunctionsTreeWidget::itemFunctionId(const QTreeWidgetItem* item) const
{
    if (item == NULL || item->parent() == NULL)
        return Function::invalidId();
    else
    {
        QVariant var = item->data(COL_NAME, Qt::UserRole);
        if (var.isValid() == false)
            return Function::invalidId();

        return var.toUInt();
    }
}

QTreeWidgetItem* FunctionsTreeWidget::functionItem(const Function* function)
{
    Q_ASSERT(function != NULL);

    QTreeWidgetItem* parent = parentItem(function);
    Q_ASSERT(parent != NULL);

    for (int i = 0; i < parent->childCount(); i++)
    {
        QTreeWidgetItem* item = parent->child(i);
        if (itemFunctionId(item) == function->id())
            return item;
        // Sequences are in a further sublevel. Check if there is any
        if (item->childCount() > 0)
        {
            for (int j = 0; j < item->childCount(); j++)
            {
                QTreeWidgetItem* seqItem = item->child(j);
                if (itemFunctionId(seqItem) == function->id())
                    return item;
            }
        }
    }

    return NULL;
}

QIcon FunctionsTreeWidget::functionIcon(const Function* function) const
{
    switch (function->type())
    {
    case Function::Scene:
        return QIcon(":/scene.png");
    case Function::Chaser:
        if (qobject_cast<const Chaser*>(function)->isSequence() == true)
            return QIcon(":/sequence.png");
        else
            return QIcon(":/chaser.png");
    case Function::EFX:
        return QIcon(":/efx.png");
    case Function::Collection:
        return QIcon(":/collection.png");
    case Function::RGBMatrix:
        return QIcon(":/rgbmatrix.png");
    case Function::Script:
        return QIcon(":/script.png");
    case Function::Show:
        return QIcon(":/show.png");
    case Function::Audio:
        return QIcon(":/audio.png");
    default:
        return QIcon(":/function.png");
    }
}

/*********************************************************************
 * Tree folders
 *********************************************************************/

void FunctionsTreeWidget::addFolder()
{
    blockSignals(true);
    if (selectedItems().isEmpty())
        return;

    QTreeWidgetItem *item = selectedItems().first();
    if (item->text(COL_PATH).isEmpty())
        item = item->parent();

    int type = item->data(COL_NAME, Qt::UserRole + 1).toInt();

    QString fullPath = item->text(COL_PATH);
    if (fullPath.endsWith('/') == false)
        fullPath.append("/");

    fullPath += "New folder";

    QTreeWidgetItem *folder = new QTreeWidgetItem(item);
    folder->setText(COL_NAME, "New folder");
    folder->setIcon(COL_NAME, QIcon(":/folder.png"));
    folder->setData(COL_NAME, Qt::UserRole, Function::invalidId());
    folder->setData(COL_NAME, Qt::UserRole + 1, type);
    folder->setText(COL_PATH, fullPath);
    folder->setFlags(folder->flags() | Qt::ItemIsDropEnabled | Qt::ItemIsEditable);

    m_foldersMap[fullPath] = folder;
    item->setExpanded(true);

    blockSignals(false);
}

void FunctionsTreeWidget::deleteFolder(QString name)
{
    if (m_foldersMap.contains(name))
        m_foldersMap.remove(name);
}

QTreeWidgetItem *FunctionsTreeWidget::folderItem(QString name)
{
    if (m_foldersMap.contains(name))
        return m_foldersMap[name];

    if (name.endsWith('/'))
        return NULL;

    qDebug() << "Folder" << name << "doesn't exist. Creating it...";

    QTreeWidgetItem *parentNode = NULL;
    int type = Function::Undefined;
    QString fullPath;
    QStringList levelsList = name.split("/");
    foreach(QString level, levelsList)
    {
        // the first round is a category node. Just retrieve the item pointer
        // and the type, then skip it.
        if (fullPath.isEmpty())
        {
            parentNode = m_foldersMap[QString(level + "/")];
            type = parentNode->data(COL_NAME, Qt::UserRole + 1).toInt();
            fullPath = level;
            continue;
        }

        fullPath.append("/");
        fullPath.append(level);

        // create only missing levels
        if (m_foldersMap.contains(fullPath) == false)
        {
            QTreeWidgetItem *folder = new QTreeWidgetItem(parentNode);
            folder->setText(COL_NAME, level);
            folder->setIcon(COL_NAME, QIcon(":/folder.png"));
            folder->setData(COL_NAME, Qt::UserRole, Function::invalidId());
            folder->setData(COL_NAME, Qt::UserRole + 1, type);
            folder->setText(COL_PATH, fullPath);
            folder->setFlags(folder->flags() | Qt::ItemIsDropEnabled | Qt::ItemIsEditable);

            m_foldersMap[fullPath] = folder;
        }
        else
            parentNode = m_foldersMap[fullPath];
    }

    return m_foldersMap[name];
}

void FunctionsTreeWidget::slotItemChanged(QTreeWidgetItem *item)
{
    blockSignals(true);
    qDebug() << "TREE item changed";
    if (item->text(COL_PATH).isEmpty())
        return;

    QTreeWidgetItem *parent = item->parent();
    if (parent != NULL)
    {
        QString fullPath = parent->text(COL_PATH);

        if (fullPath.endsWith('/') == false)
            fullPath.append("/");
        fullPath.append(item->text(COL_NAME));

        m_foldersMap.remove(item->text(COL_PATH));
        item->setText(COL_PATH, fullPath);
        m_foldersMap[fullPath] = item;
        slotUpdateChildrenPath(item);
    }
    blockSignals(false);
}

void FunctionsTreeWidget::slotUpdateChildrenPath(QTreeWidgetItem *root)
{
    if (root->childCount() == 0)
        return;
    for (int i = 0; i < root->childCount(); i++)
    {
        QTreeWidgetItem *child = root->child(i);

        // child can be a function node or another folder
        QString path = child->text(COL_PATH);
        if (path.isEmpty()) // function node
        {
            quint32 fid = child->data(COL_NAME, Qt::UserRole).toUInt();
            Function *func = m_doc->function(fid);
            if (func != NULL)
                func->setPath(root->text(COL_PATH));
        }
        else
        {
            slotItemChanged(child);
        }
    }
}

void FunctionsTreeWidget::mousePressEvent(QMouseEvent *event)
{
    m_draggedItem = itemAt(event->pos());

    QTreeWidget::mousePressEvent(event);
}


void FunctionsTreeWidget::dropEvent(QDropEvent *event)
{
    QTreeWidgetItem *dropItem = itemAt(event->pos());

    if (m_draggedItem == NULL || dropItem == NULL)
        return;

    QVariant var = dropItem->data(COL_NAME, Qt::UserRole + 1);
    if (var.isValid() == false)
        return;

    int dropType = var.toInt();
    //QString folderName = dropItem->text(COL_PATH);

    quint32 dragFID = m_draggedItem->data(COL_NAME, Qt::UserRole).toUInt();
    Function *dragFunc = m_doc->function(dragFID);
    if (dragFunc != NULL && dragFunc->type() == dropType)
    {
        QTreeWidget::dropEvent(event);
        quint32 fid = m_draggedItem->data(COL_NAME, Qt::UserRole).toUInt();
        Function *func = m_doc->function(fid);
        if (func != NULL)
            func->setPath(dropItem->text(COL_PATH));
    }
    else
    {
        // m_draggedItem is a folder
        int dragType = m_draggedItem->data(COL_NAME, Qt::UserRole + 1).toInt();
        if (dragType == dropType)
            QTreeWidget::dropEvent(event);
        slotItemChanged(m_draggedItem);
    }
}
