    case tsTriangle:
    {
      if (clip.intersects(QRectF(center-QPointF(w, w), center+QPointF(w, w)).toRect()))
      {
        QPolygonF poly;
        poly << QPointF(center.x(), center.y()-w)
             << QPointF(center.x()-w, center.y()+w)
             << QPointF(center.x()+w, center.y()+w);
        double d1 = QCPVector2D(pos).distanceSquaredToLine(poly.at(0), poly.at(1));
        double d2 = QCPVector2D(pos).distanceSquaredToLine(poly.at(1), poly.at(2));
        double d3 = QCPVector2D(pos).distanceSquaredToLine(poly.at(2), poly.at(0));
        double result = qSqrt(qMin(d1, qMin(d2, d3)));
        if (result > mParentPlot->selectionTolerance()*0.99 && mBrush.style() != Qt::NoBrush && mBrush.color().alpha() != 0)
        {
          if (poly.containsPoint(pos, Qt::OddEvenFill))
            result = mParentPlot->selectionTolerance()*0.99;
        }
        return result;
      }
      break;
    }
    case tsTriangle:
    {
      if (clip.intersects(QRectF(center-QPointF(w, w), center+QPointF(w, w)).toRect()))
      {
        QPolygonF poly;
        poly << QPointF(center.x(), center.y()-w)
             << QPointF(center.x()-w, center.y()+w)
             << QPointF(center.x()+w, center.y()+w);
        painter->drawPolygon(poly);
      }
      break;
    }
