#pragma once

#include "qgraphicsview.h"
#include "qdebug.h"

class MFuncItem;
class MLineItem;
class QGraphicsScene;
class QGraphicsItem;
class MItemMap;

int getUniqueId();

class MObjectView : public QGraphicsView
{
	Q_OBJECT

public:
	MObjectView(QWidget *parent = 0);

private:
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void obtainSceneMap(QGraphicsScene* scene, const QList<QGraphicsItem*>& excepted, MItemMap& output);
	bool fuzzyIntersected(const QPainterPath& pp, const QRectF& rect);
	void drawItemObject(QGraphicsItem* item, const QRectF& mapRect, QImage& map);

private slots:
	void funcItemPortClicked(const QString& name, bool isOutput, const QPointF& pos);
	void funcItemMove(MFuncItem* item);
	void lineItemClicked(const QString& name, const QPointF& pos);
	void thisCustomContextMenuRequested(const QPoint& pos);
	void actDeleteTriggered();

private:
	QGraphicsScene* scene;
	MLineItem* activeItem; /* 当前活动连线 */
	QPoint lastPos; /* 存储上次鼠标点击位置 */
	bool doCancelOpera; /* 点击鼠标右键是取消连线还是弹菜单 */
};

class MItemMap
{
private:
	struct Node
	{
		Node* prev;
		QPoint pos;
		int fsum;
		int gfrom;
		int hdest;
	};

public:
	bool routing(const QPointF& start, int sdir, const QPointF& end, int edir, QVector<QPointF>& path) const;

private:
	int calcHdestValue(QPoint a, QPoint b) const;
	void indexToPoint(const QVector<QPoint>& indexs, QVector<QPointF>& path) const;
	void adjustStartPoint(const QPointF& start, int dir, QVector<QPointF>& path) const;
	void adjustEndPoint(const QPointF& end, int dir, QVector<QPointF>& path) const;
	bool isInflection(const Node* curr, const QPoint& next) const;
	Node *grabOneNode(QLinkedList<Node>& memory) const;

public:
	QRectF rect;
	QImage image;
	int dotPerPixel;
};




