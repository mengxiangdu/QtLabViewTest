#include "MLvItems.h"
#include "MLvView.h"
#include "qfile.h"
#include "qjsondocument.h"
#include "qjsonarray.h"
#include "qjsonobject.h"
#include "qregularexpression.h"
#include "qpainter.h"
#include "qcursor.h"
#include "qgraphicsscene.h"
#include "qgraphicssceneevent.h"
#include "qvariantanimation.h"

/////////////////////////////////////////////////////////////////////////////////////////

uint qHash(const IDataType::IData& d)
{
    return qHash(d.dataType);
}

bool operator==(const IDataType::IData& a, const IDataType::IData& b)
{
    return a.dataType == b.dataType;
}

IDataType IDataType::inst;

const IDataType* IDataType::instance()
{
    return &inst;
}

IDataType::IDataType()
{
    QFile file(u8"data-type.json");
    file.open(QIODevice::ReadOnly);
    QJsonDocument jdoc = QJsonDocument::fromJson(file.readAll());
    QJsonArray root = jdoc.array();
    for (const auto& item : root)
    {
        QJsonObject obj = item.toObject();
        IData data;
        data.dataType = obj[u8"data-type"].toString();
        data.color = obj[u8"color"].toInt();
        data.lineStyle = obj[u8"line-style"].toInt();
        config.insert(data);
    }
}

const IDataType::IData& IDataType::find(const QString& name) const
{
    auto found = config.find({ name, 0, 0 });
    if (found == config.end())
    {
        throw std::exception("不存在的数据类型");
    }
    return *found;
}

QPen IDataType::obtainPen(short style)
{
	QPen pen;
	switch (style)
	{
	case 1:
		pen.setColor(QColor(255, 18, 0));
		pen.setBrush(QBrush(QImage(u8":/QtLabViewTest/image/pen1.png")));
		pen.setWidth(4);
		break;
	case 2:
		pen.setColor(QColor(255, 205, 0));
		break;
	case 0:
	default:
		pen.setColor(QColor(0, 0, 255));
		break;
	}
	return pen;
}

/////////////////////////////////////////////////////////////////////////////////////////

uint qHash(const IFuncSet::IData& d)
{
    return qHash(d.name);
}

bool operator==(const IFuncSet::IData& a, const IFuncSet::IData& b)
{
    return a.name == b.name;
}

IFuncSet IFuncSet::inst;

const IFuncSet* IFuncSet::instance()
{
    return &inst;
}

IFuncSet::IFuncSet()
{
    QFile file(u8"func-set.json");
    file.open(QIODevice::ReadOnly);
    QJsonParseError error;
    QJsonDocument jdoc = QJsonDocument::fromJson(file.readAll(), &error);
    QJsonArray root = jdoc.array();
    for (const auto& item : root)
    {
        QJsonObject obj = item.toObject();
        IData data;
        data.name = obj[u8"name"].toString();
        data.defaultWidth = obj[u8"default-width"].toInt();
        data.defaultHeight = obj[u8"default-height"].toInt();
        data.widthResizeable = obj[u8"width-resizeable"].toBool();
        data.heightResizeable = obj[u8"height-resizeable"].toBool();
        data.image = obj[u8"image"].toString();
        data.lastExpandable = obj[u8"last-expandable"].toBool();
        data.editable = obj[u8"editable"].toBool();
        data.initText = obj[u8"init-text"].toString();
        data.validateExpr = obj[u8"validate-expr"].toString();
        QJsonArray ports = obj[u8"port"].toArray();
        for (const auto& each : ports)
        {
            IPort onePort;
            QJsonObject pobj = each.toObject();
            onePort.name = pobj[u8"name"].toString();
            onePort.align = pobj[u8"align"].toInt();
            onePort.pos = pobj[u8"pos"].toInt();
            onePort.dataType = pobj[u8"data-type"].toString();
            onePort.isOutput = pobj[u8"is-output"].toBool();
            data.ports.push_back(onePort);
        }
        config.insert(data);
    }
}

const IFuncSet::IData& IFuncSet::find(const QString& name) const
{
    auto found = config.find({ name });
    if (found == config.end())
    {
        throw std::exception("不存在的函数");
    }
    return *found;
}

/////////////////////////////////////////////////////////////////////////////////////////

MClickableItem::MClickableItem(QGraphicsItem *parent) : 
	QGraphicsObject(parent) 
{
	setFlag(QGraphicsItem::ItemIsSelectable);
}

/////////////////////////////////////////////////////////////////////////////////////////

const int borderHitTest = 10;

MFuncItem::MFuncItem(const IFuncSet::IData* dataSrc, QGraphicsItem *parent) :
	MClickableItem(parent), flashPort(-1), ani(0)
{
	dataRef = dataSrc;
	hoverWhere = -1;
	expanding = false;
	editor = 0;
	size = QSizeF(dataRef->defaultWidth, dataRef->defaultHeight);
	initPortRegion();
	setFlag(QGraphicsItem::ItemIsMovable, true);
	setAcceptHoverEvents(true);
	setToolTip(toolTipString());

	if (dataRef->editable)
	{
		editor = new MTextItem(dataRef->initText, this);
		editor->setTextInteractionFlags(Qt::TextEditorInteraction);
		editor->hide();
		QObject::connect(dynamic_cast<MTextItem*>(editor), &MTextItem::editFinished, [this]()
		{
			QRegularExpression expr(dataRef->validateExpr);
			if (!expr.match(editor->toPlainText()).hasMatch())
			{
				editor->setPlainText(dataRef->initText);
				return;
			}
			editor->hide();
		});
	}
}

void MFuncItem::initPortRegion()
{
	for (const auto& item : dataRef->ports)
	{
		QRectF rect;
		switch (item.align)
		{
		case 0:
			rect.setX(0);
			rect.setY(item.pos);
			rect.setWidth(10);
			rect.setHeight(6);
			break;
		case 1:
			rect.setX(item.pos);
			rect.setY(0);
			rect.setWidth(6);
			rect.setHeight(10);
			break;
		case 2:
			rect.setX(size.width() - 10);
			rect.setY(item.pos);
			rect.setWidth(10);
			rect.setHeight(6);
			break;
		case 3:
			rect.setX(item.pos);
			rect.setY(size.height() - 10);
			rect.setWidth(6);
			rect.setHeight(10);
			break;
		default:
			throw std::exception("JSON文件配置错误，合法值是0~3");
			break;
		}
		portRect.push_back(rect);
	}
}

void MFuncItem::updatePortRegion()
{
	int count = portRect.size();
	for (int i = 0; i < count; i++)
	{
		int iref = std::min(dataRef->ports.size() - 1, i);
		switch (dataRef->ports[iref].align)
		{
		case 0: /* 左上端口不需移动 */
		case 1: /* 左上端口不需移动 */
			break;
		case 2:
			portRect[i].moveLeft(size.width() - 10);
			break;
		case 3:
			portRect[i].moveTop(size.height() - 10);
			break;
		default:
			throw std::exception("JSON文件配置错误，合法值是0~3");
			break;
		}
	}
}

QRectF MFuncItem::boundingRect() const
{
	return QRectF(0, 0, size.width(), size.height());
}

void MFuncItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	if (dataRef->image.isEmpty())
	{
		QVector<QPointF> dots;
		dots.push_back(QPointF(0, 0));
		dots.push_back(QPointF(0, size.height()));
		dots.push_back(QPointF(size.width(), size.height()));
		dots.push_back(QPointF(size.width(), 0));
		painter->setPen(QPen(Qt::black, 1));
		painter->setBrush(Qt::yellow);
		painter->drawPolygon(dots);
	}
	else
	{
		QImage image(dataRef->image);
		painter->drawImage(QRectF(0, 0, size.width(), size.height()), image);
	}
	drawPort(painter);
}

void MFuncItem::drawPort(QPainter *painter)
{
	painter->save();
	painter->setPen(Qt::NoPen);
	for (int i = 0; i < (int)portRect.count(); i++)
	{
		int iref = std::min(dataRef->ports.count() - 1, i);
		const IDataType* datas = IDataType::instance();
		const IDataType::IData& set = datas->find(dataRef->ports[iref].dataType);
		bool change = (flashPort == i && inOut == 1);
		painter->setOpacity(change ? 0.75 : 0.25);
		painter->setBrush(QBrush(set.color));
		painter->drawRect(portRect[i]);
	}
	painter->restore();
}

void MFuncItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
	if (editor && editor->isVisible())
	{
		/* 正在编辑则什么也不做 */
		return;
	}
	auto found = std::find_if(portRect.begin(), portRect.end(),
		[event](const QRectF& r) { return r.contains(event->pos()); });
	if (found != portRect.end())
	{
		flashPort = short(found - portRect.begin());
		beginFlash();
	}
	else
	{
		flashPort = -1;
		endFlash();
	}
	hoverWhere = checkIfExtendEdge(event->pos());
	setToolTip(toolTipString());
}

void MFuncItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
	// do nothing.
}

void MFuncItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
	endFlash();
}

//---------------------------------------------------------------------------------------
// 判断鼠标悬浮在算法的哪个位置
// pos：鼠标相对本ITEM位置
// 返回值：0~3分别指鼠标悬浮在函数的左上右下边上；4代表鼠标在函数下方的扩展算法边；负
// 数代表在其它地方
//---------------------------------------------------------------------------------------
short MFuncItem::checkIfExtendEdge(const QPointF &pos)
{
	if (flashPort >= 0)
	{
		unsetCursor();
		return -1;
	}
	/* 尾数可扩展 */
	if (dataRef->lastExpandable)
	{
		if (pos.y() > size.height() - borderHitTest)
		{
			setCursor(Qt::SizeVerCursor);
			return 4;
		}
	}
	/* 宽可拉伸 */
	if (dataRef->widthResizeable)
	{
		if (pos.x() < borderHitTest)
		{
			setCursor(Qt::SizeHorCursor);
			return 0;
		}
		if (pos.x() > size.width() - borderHitTest)
		{
			setCursor(Qt::SizeHorCursor);
			return 2;
		}
	}
	/* 高可拉伸 */
	if (dataRef->heightResizeable)
	{
		if (pos.y() < borderHitTest)
		{
			setCursor(Qt::SizeVerCursor);
			return 1;
		}
		if (pos.y() > size.height() - borderHitTest)
		{
			setCursor(Qt::SizeVerCursor);
			return 3;
		}
	}
	unsetCursor();
	return -1;
}

void MFuncItem::beginFlash()
{
	if (!ani)
	{
		ani = new QVariantAnimation(this);
		ani->setLoopCount(-1);
		ani->setStartValue(0);
		ani->setEndValue(2);
		ani->setDuration(800);
		QObject::connect(ani, &QVariantAnimation::valueChanged,
			[this](const QVariant& value) { setUpdateAnimation(value.toInt()); });
		ani->start(QAbstractAnimation::DeleteWhenStopped);
	}
}

void MFuncItem::endFlash()
{
	if (ani)
	{
		ani->stop();
		ani = 0;
		inOut = 0;
	}
	update();
}

void MFuncItem::setUpdateAnimation(int i)
{
	inOut = i;
	update();
}

QString MFuncItem::toolTipString() const
{
	if (flashPort < 0)
	{
		return objectName();
	}
	QString marker;
	int iref = flashPort;
	if (iref >= dataRef->ports.count())
	{
		iref = dataRef->ports.count() - 1;
		marker = u8"（复制的）";
	}
	QString str;
	str = u8"端口描述\n";
	str += u8"名称：" + dataRef->ports[iref].name + marker + u8"\n";
	str += dataRef->ports[iref].isOutput ? u8"类别：输出\n" : u8"类别：输入\n";
	str += u8"数据：" + dataRef->ports[iref].dataType;
	return str;
}

void MFuncItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	if (hoverWhere >= 0)
	{
		expanding = true;
		return;
	}
	QGraphicsItem::mousePressEvent(event);
}

void MFuncItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	if (!expanding)
	{
		QGraphicsItem::mouseMoveEvent(event);
		emit move(this);
		return;
	}
	int lastH = calcLastPortHeight();
	prepareGeometryChange();
	QPointF opos = pos();
	int length;
	switch (hoverWhere)
	{
	case 0:
		length = event->pos().x() - event->lastPos().x();
		opos.setX(opos.x() + length);
		setPos(opos);
		length = size.width() - length;
		size.setWidth(std::max(dataRef->defaultWidth, length));
		break;
	case 1:
		length = event->pos().y() - event->lastPos().y();
		opos.setY(opos.y() + length);
		setPos(opos);
		length = size.height() - length;
		size.setHeight(std::max(dataRef->defaultHeight, length));
		break;
	case 2:
		length = event->pos().x() - event->lastPos().x();
		length = size.width() + length;
		size.setWidth(std::max(dataRef->defaultWidth, length));
		break;
	case 3:
		length = event->pos().y() - event->lastPos().y();
		length = size.height() + length;
		size.setHeight(std::max(dataRef->defaultHeight, length));
		break;
	case 4:
		/* 删除引脚 */
		if (event->pos().y() - size.height() < -lastH / 2 &&
			portRect.count() > dataRef->ports.count())
		{
			size.setHeight(size.height() - lastH);
			portRect.pop_back();
		}
		/* 添加引脚 */
		if (event->pos().y() - size.height() > lastH / 2)
		{
			size.setHeight(size.height() + lastH);
			portRect.push_back(portRect.back().adjusted(0, lastH, 0, lastH));
		}
		break;
	default:
		throw std::exception("错误的操作");
		break;
	}
	updatePortRegion();
}

void MFuncItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	expanding = false;
	if (hoverWhere < 0 && flashPort >= 0)
	{
		const IFuncSet::IPort& reference = getPortInfo(flashPort);
		QString name = QString(u8"%1-%3-%2").arg(objectName()).arg(flashPort).
			arg(reference.isOutput ? u8"out" : u8"in");
		emit portClicked(name, reference.isOutput, event->scenePos());
	}
	QGraphicsItem::mouseReleaseEvent(event);
}

const IFuncSet::IPort& MFuncItem::getPortInfo(int index) const
{
	int realIndex = std::min(index, dataRef->ports.size() - 1);
	return dataRef->ports[realIndex];
}

int MFuncItem::calcLastPortHeight() const
{
	QRectF last = portRect.last();
	int midy = last.top();
	return size.height() - midy;
}

void MFuncItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
	if (editor)
	{
		editor->show();
	}
}

IPortInfo MFuncItem::findPortRect(int index) const
{
	IPortInfo info;
	int iref = std::min(dataRef->ports.count() - 1, index);
	const IDataType* dtype = IDataType::instance();
	const IDataType::IData& ddata = dtype->find(dataRef->ports[iref].dataType);
	info.lineStyle = ddata.lineStyle;
	info.dir = dataRef->ports[iref].align;
	info.isOutput = dataRef->ports[iref].isOutput;
	info.rect = mapRectToScene(portRect[index]);
	switch (info.dir)
	{
	case 0:
		info.pos = QPointF(info.rect.left(), info.rect.center().y());
		break;
	case 1:
		info.pos = QPointF(info.rect.center().x(), info.rect.top());
		break;
	case 2:
		info.pos = QPointF(info.rect.right(), info.rect.center().y());
		break;
	case 3:
		info.pos = QPointF(info.rect.center().x(), info.rect.bottom());
		break;
	default:
		throw std::exception("错误的JSON数据");
		break;
	}
	return info;
}

bool MFuncItem::connectTo(MClickableItem* item) const
{
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////

MTextItem::MTextItem(const QString& str, QGraphicsItem *parent) :
	QGraphicsTextItem(str, parent)
{
}

QRectF MTextItem::boundingRect() const
{
	return parentItem()->boundingRect();
}

void MTextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	painter->setPen(Qt::blue);
	painter->setBrush(Qt::white);
	painter->drawRect(boundingRect());
	QGraphicsTextItem::paint(painter, option, widget);
}

void MTextItem::focusInEvent(QFocusEvent *event)
{
	QGraphicsTextItem::focusInEvent(event);
}

void MTextItem::focusOutEvent(QFocusEvent *event)
{
	QGraphicsTextItem::focusOutEvent(event);
	emit editFinished();
}

/////////////////////////////////////////////////////////////////////////////////////////

const qreal halfWidth = 3;

//---------------------------------------------------------------------------------------
// name组成是：函数名-输入输出-端口序号。如果函数名是null则端口序号表示点坐标
//---------------------------------------------------------------------------------------
IPortInfo find(QGraphicsScene* scene, const QString& name)
{
	QStringList parts = name.split('-');
	if (parts[0] == u8"null")
	{
		int index = parts[2].indexOf(',');
		IPortInfo info;
		info.dir = -1;
		info.isOutput = false;
		info.lineStyle = -1;
		info.pos.setX(parts[2].left(index).toDouble());
		info.pos.setY(parts[2].mid(index + 1).toDouble());
		info.rect.setX(info.pos.x() - halfWidth);
		info.rect.setY(info.pos.y() - halfWidth);
		info.rect.setWidth(halfWidth * 2);
		info.rect.setHeight(halfWidth * 2);
		return info;
	}
	QList<QGraphicsItem*> children = scene->items();
	auto found = std::find_if(children.begin(), children.end(), [&parts](const QGraphicsItem* g) -> bool
	{
		const MClickableItem* obj = dynamic_cast<const MClickableItem*>(g);
		return obj ? obj->objectName() == parts[0] : false;
	});
	if (found == children.end())
	{
		throw std::exception("错误的函数名称");
	}
	MClickableItem* funcItem = dynamic_cast<MClickableItem*>(*found);
	return funcItem->findPortRect(parts[2].toInt());
}

/////////////////////////////////////////////////////////////////////////////////////////

MLineItem::MLineItem(QGraphicsItem *parent) :
	MClickableItem(parent), myPen(Qt::NoPen)
{
	setToolTip(objectName());
}

QRectF MLineItem::getBoundingRect(const QVector<QPointF>& points) const
{
	qreal x1 = DBL_MAX, x2 = -DBL_MAX;
	qreal y1 = DBL_MAX, y2 = -DBL_MAX;
	for (const auto& item : points)
	{
		if (item.x() < x1)
		{
			x1 = item.x();
		}
		if (item.x() > x2)
		{
			x2 = item.x();
		}
		if (item.y() < y1)
		{
			y1 = item.y();
		}
		if (item.y() > y2)
		{
			y2 = item.y();
		}
	}
	return QRectF(x1, y1, x2 - x1, y2 - y1);
}

QRectF MLineItem::boundingRect() const
{
	QRectF rect;
	if (path.empty())
	{
		IPortInfo port1 = find(scene(), portName1);
		IPortInfo port2 = find(scene(), portName2);
		qreal x1 = qMin(port1.pos.x(), port2.pos.x());
		qreal x2 = qMax(port1.pos.x(), port2.pos.x());
		qreal y1 = qMin(port1.pos.y(), port2.pos.y());
		qreal y2 = qMax(port1.pos.y(), port2.pos.y());
		rect = QRectF(x1, y1, x2 - x1, y2 - y1);
		return rect;
	}
	rect = getBoundingRect(path);
	rect.adjust(-halfWidth, -halfWidth, halfWidth, halfWidth);
	return rect;
}

QPainterPath MLineItem::shape() const
{
	QPainterPath spath;
	spath.setFillRule(Qt::WindingFill);
	for (int i = 1; i < path.count(); i++)
	{
		if (path[i - 1].x() == path[i].x())
		{
			QRectF rect;
			rect.setX(path[i].x() - halfWidth);
			rect.setY(std::min(path[i - 1].y(), path[i].y()) - halfWidth);
			rect.setWidth(2 * halfWidth);
			rect.setHeight(fabs(path[i - 1].y() - path[i].y()) + 2 * halfWidth);
			spath.addRoundedRect(rect, halfWidth, halfWidth);
		}
		else if (path[i - 1].y() == path[i].y())
		{
			QRectF rect;
			rect.setY(path[i].y() - halfWidth);
			rect.setX(std::min(path[i - 1].x(), path[i].x()) - halfWidth);
			rect.setHeight(2 * halfWidth);
			rect.setWidth(fabs(path[i - 1].x() - path[i].x()) + 2 * halfWidth);
			spath.addRoundedRect(rect, halfWidth, halfWidth);
		}
	}
	return spath;
}

void MLineItem::setStartPort(const QString& name)
{
	portName1 = name;
}

void MLineItem::setEndPort(const QString& name)
{
	prepareGeometryChange();
	portName2 = name;
}

bool MLineItem::isStartOutput() const
{
	QRegularExpression expr(u8R"(-((in)|(out))-)");
	QRegularExpressionMatch result = expr.match(portName1);
	if (result.hasMatch() && 
		result.captured(1) == u8"out")
	{
		return true;
	}
	return false;
}

void MLineItem::routing(const MItemMap& map)
{
	prepareGeometryChange();
	IPortInfo port1 = find(scene(), portName1);
	IPortInfo port2 = find(scene(), portName2);
	path.clear();
	map.routing(port1.pos, port1.dir, port2.pos, port2.dir, path);
}

QString MLineItem::startObject() const
{
	QStringList strs = portName1.split('-');
	if (strs[0] != u8"null")
	{
		return strs[0];
	}
	return QString();
}

QString MLineItem::endObject() const
{
	QStringList strs = portName2.split('-');
	if (strs[0] != u8"null")
	{
		return strs[0];
	}
	return QString();
}

void MLineItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	if (myPen.style() == Qt::NoPen)
	{
		IPortInfo info = find(scene(), portName1);
		myPen = IDataType::obtainPen(info.lineStyle);
	}
	if (path.isEmpty())
	{
		IPortInfo port1 = find(scene(), portName1);
		IPortInfo port2 = find(scene(), portName2);
		painter->setRenderHint(QPainter::Antialiasing);
		painter->setPen(QPen(Qt::gray, 1, Qt::DashLine));
		painter->drawLine(port1.pos, port2.pos);
	}
	else /* 有路径 */
	{
		painter->setPen(myPen);
		painter->drawPolyline(path);
		painter->setRenderHint(QPainter::Antialiasing);
		painter->setPen(QPen(myPen.color(), 4, Qt::SolidLine, Qt::RoundCap));
		/* 如果是连到线上就在端点处画实心圆点 */
		if (portName1.startsWith(u8"线"))
		{
			painter->drawPoint(path.first());
		}
		if (portName2.startsWith(u8"线"))
		{
			painter->drawPoint(path.last());
		}
	}
	painter->fillPath(shape(), QColor(255, 127, 0, 64));
}

void MLineItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	lastPos = event->scenePos();
}

void MLineItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	QPointF pos = event->scenePos();
	if ((lastPos - pos).manhattanLength() < halfWidth)
	{
		int offset = findNearestPos(pos);
		emit clicked(QString(u8"%1-out-%2").arg(objectName()).arg(offset), pos);
	}
}

#define EPSILON 0.001

//---------------------------------------------------------------------------------------
// 找离鼠标最近的路径位置。返回从起始点沿路径到鼠标位置的距离
//---------------------------------------------------------------------------------------
int MLineItem::findNearestPos(const QPointF& pos)
{
	int count = path.size();
	qreal subDist = 0;
	for (int i = 1; i < count; i++)
	{
		qreal xdir = qAbs(path[i].x() - path[i - 1].x());
		qreal ydir = qAbs(path[i].y() - path[i - 1].y());
		qreal xdist = qAbs(pos.x() - path[i - 1].x());
		qreal ydist = qAbs(pos.y() - path[i - 1].y());
		if (ydir == 0 && xdist >= 0 && xdist < xdir && ydist < halfWidth + EPSILON)
		{
			subDist += xdist;
			break;
		}
		if (xdir == 0 && ydist >= 0 && ydist < ydir && xdist < halfWidth + EPSILON)
		{
			subDist += ydist;
			break;
		}
		subDist += xdir + ydir;
	}
	return (int)subDist;
}

#undef EPSILON

IPortInfo MLineItem::findPortRect(int offset) const
{
	IPortInfo info;
	int count = path.size();
	for (int i = 1; i < count; i++)
	{
		qreal xdir = path[i].x() - path[i - 1].x();
		qreal ydir = path[i].y() - path[i - 1].y();
		qreal dist = qAbs(xdir) + qAbs(ydir);
		offset -= dist;
		if (offset < 0)
		{
			info.dir = -1;
			info.isOutput = true;
			info.lineStyle = -1;
			info.pos = path[i] + offset / dist * QPointF(xdir, ydir);
			info.rect = QRectF();
			break;
		}
	}
	return info;
}

bool MLineItem::connectTo(MClickableItem* item) const
{
	QString title = item->objectName();
	bool testPort1 = portName1.startsWith(title);
	bool testPort2 = portName2.startsWith(title);
	return testPort1 || testPort2;
}





