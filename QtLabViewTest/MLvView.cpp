#include "MLvView.h"
#include "MLvItems.h"
#include "qlinkedlist.h"
#include "qgraphicsscene.h"
#include "qmimedata.h"
#include "qaction.h"
#include "qevent.h"
#include "qmenu.h"

/* 设置Z顺序有助于生成地图 */
/* 默认的降序排列会把Func Item排在前面 */
#define FUNC_ITEM_ZORDER 1000
#define LINE_ITEM_ZORDER 500

static int id = 1;

int getUniqueId()
{
	return id++;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////

MObjectView::MObjectView(QWidget *parent) : 
	QGraphicsView(parent), lastPos(0xDEADBEEF, 0xDEADBEEF)
{
    setMouseTracking(true);
    scene = new QGraphicsScene(this);
	scene->setSceneRect(0, 0, 10480, 10480);
    setScene(scene);

	activeItem = 0;
	doCancelOpera = false;

	connect(this, &MObjectView::customContextMenuRequested, this, &MObjectView::thisCustomContextMenuRequested);

	QAction* actDelete = new QAction(u8"删除(&D)", this);
	connect(actDelete, &QAction::triggered, this, &MObjectView::actDeleteTriggered);
	actDelete->setShortcut(QKeySequence::Delete);
	addAction(actDelete);
}

void MObjectView::actDeleteTriggered()
{
	QVector<MClickableItem*> stack;
	for (auto itOne : items())
	{
		MClickableItem* clickItem = dynamic_cast<MClickableItem*>(itOne);
		/* MTextItem不继承自MClickableItem */
		if (clickItem && clickItem->isSelected())
		{
			stack.push_back(clickItem);
		}
	}
	/* 查找所有和被选中Item相关的Item，深度优先 */
	QVector<MClickableItem*> dels;
	while (!stack.isEmpty())
	{
		MClickableItem* temp = stack.takeLast();
		dels.push_back(temp);
		for (auto itOne : items())
		{
			MClickableItem* clickItem = dynamic_cast<MClickableItem*>(itOne);
			if (clickItem && clickItem->connectTo(temp))
			{
				stack.push_back(clickItem);
			}
		}
	}
	/* 删除相关的Item */
	for (auto itOne : dels)
	{
		scene->removeItem(itOne);
	}
}

void MObjectView::thisCustomContextMenuRequested(const QPoint& pos)
{
	if (!doCancelOpera)
	{
		QMenu menu;
		menu.addActions(actions());
		menu.exec(mapToGlobal(pos));
	}
	doCancelOpera = false;
}

void MObjectView::dragEnterEvent(QDragEnterEvent *event)
{
    event->accept();
}

void MObjectView::dragMoveEvent(QDragMoveEvent *event)
{
    event->accept();
}

void MObjectView::dropEvent(QDropEvent *event)
{
    const QMimeData* data = event->mimeData();
    QStringList strs = data->formats();
    QByteArray arrs = data->data(strs[0]);
    QDataStream ds(&arrs, QIODevice::ReadOnly);
    while (!ds.atEnd())
    {
        int row, col;
        QMap<int, QVariant> roleDataMap;
        ds >> row >> col >> roleDataMap;

        const IFuncSet* funcs = IFuncSet::instance();
        const IFuncSet::IData& funcData = funcs->find(roleDataMap.value(0).toString());
        MFuncItem* item = new MFuncItem(&funcData);
        item->setObjectName(funcData.name + QString::number(getUniqueId()));
        item->setZValue(FUNC_ITEM_ZORDER);
        connect(item, &MFuncItem::portClicked, this, &MObjectView::funcItemPortClicked);
        connect(item, &MFuncItem::move, this, &MObjectView::funcItemMove);
        scene->addItem(item);

        QPointF scenePos = mapToScene(event->pos()); /* 相对于场景的位置 */
        item->setPos(scenePos);
    }
    event->accept();
}

void MObjectView::mousePressEvent(QMouseEvent* event)
{
	if ((event->buttons() & Qt::RightButton) && 
		activeItem)
	{
		lastPos = event->pos();
	}
	QGraphicsView::mousePressEvent(event);
}

void MObjectView::mouseReleaseEvent(QMouseEvent* event)
{
	if ((lastPos - event->pos()).manhattanLength() < 3 &&
		activeItem)
	{
		/* 点击右键取消连线 */
		scene->removeItem(activeItem);
		lastPos = QPoint(0xDEADBEEF, 0xDEADBEEF);
		activeItem = 0;
		doCancelOpera = true;
	}
	QGraphicsView::mouseReleaseEvent(event);
}

void MObjectView::mouseMoveEvent(QMouseEvent *event)
{
    if (activeItem)
    {
        QPointF pos = mapToScene(event->pos());
        activeItem->setEndPort(QString(u8"null-in-%1,%2").arg(pos.x()).arg(pos.y()));
    }
    QGraphicsView::mouseMoveEvent(event);
}

void MObjectView::funcItemPortClicked(const QString& name, bool isOutput, const QPointF& pos)
{
    if (activeItem)
    {
		/* 连线的两端必须一个输入一个输出 */
		if (activeItem->isStartOutput() ^ isOutput)
		{
			MItemMap map;
			obtainSceneMap(scene, QList<QGraphicsItem*>(), map);
			activeItem->setEndPort(name);
			activeItem->routing(map);
			activeItem = 0;
		}
    }
    else
    {
        activeItem = new MLineItem;
        activeItem->setObjectName(u8"线" + QString::number(getUniqueId()));
        activeItem->setStartPort(name);
        activeItem->setEndPort(QString(u8"null-in-%1,%2").arg(pos.x()).arg(pos.y()));
        activeItem->setZValue(LINE_ITEM_ZORDER);
        connect(activeItem, &MLineItem::clicked, this, &MObjectView::lineItemClicked);
        scene->addItem(activeItem);
    }
}

void MObjectView::lineItemClicked(const QString& portName, const QPointF& pos)
{
    if (!activeItem)
    {
		qDebug() << u8"clicked: " << portName;
		QGraphicsItem* src = dynamic_cast<QGraphicsItem*>(sender());
        activeItem = new MLineItem(src);
        activeItem->setObjectName(u8"线" + QString::number(getUniqueId()));
        activeItem->setStartPort(portName);
        activeItem->setEndPort(QString(u8"null-in-%1,%2").arg(pos.x()).arg(pos.y()));
        activeItem->setZValue(LINE_ITEM_ZORDER);
        connect(activeItem, &MLineItem::clicked, this, &MObjectView::lineItemClicked);
        //scene->addItem(activeItem);
    }
}

//---------------------------------------------------------------------------------------
// 在鼠标拖动函数图标移动时要更新相关的连线
//---------------------------------------------------------------------------------------
void MObjectView::funcItemMove(MFuncItem* item)
{
    QList<QGraphicsItem*> objItem = scene->items();
    QList<QGraphicsItem*> needUpdate;
    for (auto obj : objItem)
    {
        MLineItem* lineItem = dynamic_cast<MLineItem*>(obj);
        if (lineItem && (lineItem->startObject() == item->objectName() || 
            lineItem->endObject() == item->objectName()))
        {
            needUpdate.append(obj);
        }
    }
	/* 更新相关连线的路径 */
    while (!needUpdate.empty())
    {
        MItemMap map;
		MLineItem* oneItem = dynamic_cast<MLineItem*>(needUpdate.back());
		obtainSceneMap(scene, needUpdate, map);
        oneItem->routing(map);
        needUpdate.pop_back();
    }
}

bool MObjectView::fuzzyIntersected(const QPainterPath& pp, const QRectF& rect)
{
    QPainterPath temp;
    temp.addRect(rect);
    QRectF rf = pp.intersected(temp).boundingRect();
    bool cross = (rf.width() >= 0.01 && rf.height() >= 0.01);
    return cross;
}

void MObjectView::drawItemObject(QGraphicsItem* item, const QRectF& mapRect, QImage& map)
{
	QRectF rect = item->mapRectToScene(item->boundingRect());
	QPainterPath itemShape = item->mapToScene(item->shape());
	int minx = std::floorf((rect.left() - mapRect.left()) / 8.0f);
	int miny = std::floorf((rect.top() - mapRect.top()) / 8.0f);
	int maxx = std::ceilf((rect.right() - mapRect.left()) / 8.0f);
	int maxy = std::ceilf((rect.bottom() - mapRect.top()) / 8.0f);
	for (int i = miny; i < maxy; i++)
	{
		for (int j = minx; j < maxx; j++)
		{
			QRectF cell(mapRect.left() + j * 8, mapRect.top() + i * 8, 8, 8);
			bool cross = fuzzyIntersected(itemShape, cell);
			if (cross && dynamic_cast<MFuncItem*>(item))
			{
				map.setPixelColor(j, i, QColor(255, 255, 255));
			}
			else if (cross && dynamic_cast<MLineItem*>(item))
			{
				map.setPixelColor(j, i, QColor(1, 1, 1));
			}
		}
	}
}
#include "qelapsedtimer.h"
void MObjectView::obtainSceneMap(QGraphicsScene* scene, const QList<QGraphicsItem*>& excepted, MItemMap& output)
{
    QRectF rect = scene->itemsBoundingRect();
    rect.adjust(-40, -40, 40, 40);
    int wmap = (int)rect.width() / 8 + 1;
    int hmap = (int)rect.height() / 8 + 1;
    QImage image(wmap, hmap, QImage::Format_Grayscale8);
    image.fill(Qt::black);
    QList<QGraphicsItem*> objItem = scene->items(); /* 默认的降序排列将使Func排在Line前面 */
	qDebug() << u8"=======";
    for (auto item : objItem)
    {
	QElapsedTimer timer;
	timer.start();
		/* MTextItem不参与建造地图 */
        if (!dynamic_cast<MTextItem*>(item) && !excepted.contains(item))
        {
			drawItemObject(item, rect, image);
        }
	qDebug() << timer.elapsed() << u8"ms" << item;
    }
    QPainter painter(&image);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(Qt::white);
    painter.drawRect(image.rect().adjusted(0, 0, -1, -1));
    output.image = std::move(image);
    output.rect = rect;
    output.dotPerPixel = 8;
}

/////////////////////////////////////////////////////////////////////////////////////////

int MItemMap::calcHdestValue(QPoint a, QPoint b) const
{
    return qAbs(a.x() - b.x()) + qAbs(a.y() - b.y());
}

bool MItemMap::routing(const QPointF& start, int sdir, const QPointF& end, int edir, QVector<QPointF>& path) const
{
    int sx = (int)((start.x() - rect.x()) / dotPerPixel);
    int sy = (int)((start.y() - rect.y()) / dotPerPixel);
    int ex = (int)((end.x() - rect.x()) / dotPerPixel);
    int ey = (int)((end.y() - rect.y()) / dotPerPixel);

	QLinkedList<Node> buffer;
    QVector<Node*> openList, closeList;
    Node* firstNode = grabOneNode(buffer);
    firstNode->prev = 0;
    firstNode->pos = QPoint(sx, sy);
    firstNode->gfrom = 0;
    firstNode->hdest = calcHdestValue(QPoint(sx, sy), QPoint(ex, ey));
    firstNode->fsum = firstNode->hdest;
    openList.push_back(firstNode);

    while (!openList.empty())
    {
        Node* curr = *std::min_element(openList.begin(), openList.end(), 
            [](const Node* a, const Node* b) { return a->fsum < b->fsum; });
        closeList.push_back(curr);
        openList.removeOne(curr);

        QPoint ns[4]; /* 无需检查越界 */
        ns[0] = QPoint(curr->pos.x(), curr->pos.y() - 1);
        ns[1] = QPoint(curr->pos.x() + 1, curr->pos.y());
        ns[2] = QPoint(curr->pos.x(), curr->pos.y() + 1);
        ns[3] = QPoint(curr->pos.x() - 1, curr->pos.y());
        for (const QPoint& nei : ns)
        {
            if (nei == QPoint(ex, ey))
            {
                QVector<QPoint> route;
                route.push_front(nei);
                while (curr)
                {
                    route.push_front(curr->pos);
                    curr = curr->prev;
                }
                indexToPoint(route, path);
                adjustStartPoint(start, sdir, path);
                adjustEndPoint(end, edir, path);
                return true;
            }
            int object = qGray(image.pixel(nei));
            if (object == 255)
            {
                continue; /* 不可通过 */
            }
            auto foundInClose = std::find_if(closeList.begin(), closeList.end(),
                [nei](const Node* x) { return x->pos == nei; });
            if (foundInClose != closeList.end())
            {
                continue; /* 在CLOSE表中 */
            }
            auto foundInOpen = std::find_if(openList.begin(), openList.end(),
                [nei](const Node* x) { return x->pos == nei; });
            bool corner = isInflection(curr, nei);
            int gnew = curr->gfrom + corner + object + 1;
            if (foundInOpen != openList.end()) /* 在OPEN表中 */
            {
                if (gnew < (*foundInOpen)->gfrom) /* 比较G值 */
                {
                    (*foundInOpen)->gfrom = gnew;
                    (*foundInOpen)->fsum = gnew + (*foundInOpen)->hdest;
                    (*foundInOpen)->prev = curr;
                }
            }
            else /* 不在OPEN表中 */
            {
                Node* newNode = grabOneNode(buffer);
                newNode->prev = curr;
                newNode->pos = nei;
                /* 如果此处不是障碍物，那么他的像素值代表它的权重 */
                newNode->gfrom = gnew;
                newNode->hdest = calcHdestValue(nei, QPoint(ex, ey));
                newNode->fsum = gnew + newNode->hdest;
                openList.push_back(newNode);
            }
        }
    }
    return false;
}

void MItemMap::indexToPoint(const QVector<QPoint>& indexs, QVector<QPointF>& path) const
{
    QVector<QPoint> simplized;
    int last = indexs.size() - 1;
    /* 先简化压缩路径点数量，只保留拐点 */
    for (int i = 1; i < last; i++)
    {
        int dx1 = indexs[i].x() - indexs[i - 1].x();
        int dy1 = indexs[i].y() - indexs[i - 1].y();
        int dx2 = indexs[i + 1].x() - indexs[i].x();
        int dy2 = indexs[i + 1].y() - indexs[i].y();
        if (dx1 != dx2 || dy1 != dy2)
        {
            simplized.append(indexs[i]);
        }
    }
    simplized.push_front(indexs[0]);
    simplized.push_back(indexs[last]);
    /* 然后再转换为像素位置 */
    for (const auto& item : simplized)
    {
        QPointF pos;
        pos.setX(rect.x() + (item.x() + 0.5) * dotPerPixel);
        pos.setY(rect.y() + (item.y() + 0.5) * dotPerPixel);
        path.append(pos);
    }
    if (path.size() == 2)
    {
        qreal x = (path.front().x() + path.back().x()) / 2;
        qreal y = (path.front().y() + path.back().y()) / 2;
        path.insert(1, 2, QPointF(x, y));
    }
}

//---------------------------------------------------------------------------------------
// 判断当前选择的路径是不是拐点
//---------------------------------------------------------------------------------------
bool MItemMap::isInflection(const Node* curr, const QPoint& next) const
{
    if (curr->prev)
    {
        int dx1 = next.x() - curr->pos.x();
        int dx2 = curr->pos.x() - curr->prev->pos.x();
        return dx1 != dx2;
    }
    return false;
}

void MItemMap::adjustStartPoint(const QPointF& start, int dir, QVector<QPointF>& path) const
{
    QPointF& p1 = path[0];
    QPointF& p2 = path[1];
    if (dir == 0 || dir == 2) /* 横着的 */
    {
        if (qAbs(p1.x() - p2.x()) > qAbs(p1.y() - p2.y())) /* 横着的 */
        {
            p1 = start;
            p2.setY(start.y());
        }
        else /* 竖直的 */
        {
            p1.setY(start.y());
            path.push_front(start);
        }
    }
    else /* 竖着的 */
    {
        if (qAbs(p1.x() - p2.x()) > qAbs(p1.y() - p2.y())) /* 横着的 */
        {
            p1.setX(start.x());
            path.push_front(start);
        }
        else /* 竖直的 */
        {
            p1 = start;
            p2.setX(start.x());
        }
    }
}

void MItemMap::adjustEndPoint(const QPointF& end, int dir, QVector<QPointF>& path) const
{
    QPointF& p1 = path[path.size() - 1];
    QPointF& p2 = path[path.size() - 2];
    if (dir == 0 || dir == 2) /* 横着的 */
    {
        if (qAbs(p1.x() - p2.x()) > qAbs(p1.y() - p2.y())) /* 横着的 */
        {
            p1 = end;
            p2.setY(end.y());
        }
        else /* 竖直的 */
        {
            p1.setY(end.y());
            path.push_back(end);
        }
    }
    else /* 竖着的 */
    {
        if (qAbs(p1.x() - p2.x()) > qAbs(p1.y() - p2.y())) /* 横着的 */
        {
            p1.setX(end.x());
            path.push_front(end);
        }
        else /* 竖直的 */
        {
            p1 = end;
            p2.setX(end.x());
        }
    }
}

MItemMap::Node *MItemMap::grabOneNode(QLinkedList<Node>& memory) const
{
	memory.push_back(Node());
	return &memory.back();
}











