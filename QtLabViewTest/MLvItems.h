#pragma once

#include "qgraphicsitem.h"
#include "qpen.h"
#include "qdebug.h"

class MItemMap;

class IDataType
{
public:
    struct IData
    {
        QString dataType;
        QRgb color;
        int lineStyle;
    };

public:
    static const IDataType* instance();
    const IData& find(const QString& name) const;
	static QPen obtainPen(short style);

private:
    IDataType();
    friend uint qHash(const IData& d);
    friend bool operator==(const IData& a, const IData& b);

private:
    static IDataType inst;
    QSet<IData> config;
};

class IFuncSet
{
public:
    struct IPort 
    {
        QString name;
        short align;
        short pos;
        QString dataType;
        bool isOutput;
    };

    struct IData
    {
        QString name;
        int defaultWidth;
        int defaultHeight;
        bool widthResizeable;
        bool heightResizeable;
        QString image;
        bool lastExpandable;
        bool editable;
        QString initText;
        QString validateExpr;
        QVector<IPort> ports;
    };

public:
    static const IFuncSet* instance();
    const IData& find(const QString& name) const;

private:
    IFuncSet();
    friend uint qHash(const IData& d);
    friend bool operator==(const IData& a, const IData& b);

private:
    static IFuncSet inst;
    QSet<IData> config;
};

struct IPortInfo
{
    QPointF pos;
    QRectF rect;
    short lineStyle;
    short dir;
    bool isOutput;
};

class QVariantAnimation;
class QGraphicsTextItem;

class MClickableItem : public QGraphicsObject
{
    Q_OBJECT

public:
	MClickableItem(QGraphicsItem *parent = 0);
    virtual IPortInfo findPortRect(int value) const = 0;
	virtual bool connectTo(MClickableItem* item) const = 0;
};

class MFuncItem : public MClickableItem
{
    Q_OBJECT

public:
    MFuncItem(const IFuncSet::IData* dataSrc, QGraphicsItem *parent = 0);
    QRectF boundingRect() const override;
    IPortInfo findPortRect(int index) const override;
	bool connectTo(MClickableItem* item) const override;

signals:
    void portClicked(const QString& name, bool isOutput, const QPointF& pos);
    void move(MFuncItem* object);

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = Q_NULLPTR) override;
    void drawPort(QPainter *painter);
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void initPortRegion();
    void updatePortRegion();
    void beginFlash();
    void endFlash();
    void setUpdateAnimation(int i);
    QString toolTipString() const;
    int calcLastPortHeight() const;
    short checkIfExtendEdge(const QPointF &pos);
	const IFuncSet::IPort& getPortInfo(int index) const;

private:
    const IFuncSet::IData* dataRef;
    /* 本ITEM大小 */
    QSizeF size;
    /* 端口位置矩形 */
    QVector<QRectF> portRect;
    /* 此3个变量用于动画 */
    QVariantAnimation* ani;
    short flashPort;
    short inOut;
    /* 此2个变量用于调整边缘 */
    short hoverWhere;
    bool expanding;
    /* 此变量用于编辑Item */
    QGraphicsTextItem* editor;
};

class MTextItem : public QGraphicsTextItem
{
    Q_OBJECT

public:
    MTextItem(const QString& str, QGraphicsItem *parent = 0);
    QRectF boundingRect() const override;

signals:
    void editFinished();

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = Q_NULLPTR) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
};

class MLineItem : public MClickableItem
{
    Q_OBJECT

public:
    MLineItem(QGraphicsItem *parent = 0);
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    IPortInfo findPortRect(int offset) const override;
	bool connectTo(MClickableItem* item) const override;
    void setStartPort(const QString& name);
    void setEndPort(const QString& name);
    QString startObject() const;
    QString endObject() const;
	bool isStartOutput() const;
    void routing(const MItemMap& map);

signals:
    void clicked(const QString& portName, const QPointF& pos);

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = Q_NULLPTR) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    int findNearestPos(const QPointF& pos);
	QRectF getBoundingRect(const QVector<QPointF>& points) const;

private:
    QString portName1;
    QString portName2;
    QVector<QPointF> path;
	QPen myPen;
	/* 下面是鼠标点击消息之用 */
	QPointF lastPos;
};




