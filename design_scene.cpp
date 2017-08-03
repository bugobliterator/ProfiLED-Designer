#include "design_scene.h"

design_scene::design_scene(QObject * parent) :
    QGraphicsScene(parent),
    coord_step(10),
    num_leds(0),
    repos_event(false)
{
}

void design_scene::set_led_color(qint8 led_id, QColor color)
{
    if(get_led_byid(led_id) != nullptr) {
        get_led_byid(led_id)->setBrush(QBrush(color, Qt::SolidPattern));
    }
}

void design_scene::mousePressEvent(QGraphicsSceneMouseEvent * mouseEvent)
{
    QPointF pt = get_snap_coords(QPointF(mouseEvent->scenePos().x(), mouseEvent->scenePos().y()));
    qint8 led_id = led_at_pos(pt);
    if(led_id == -1) {
        add_led(this->addEllipse(pt.x(), pt.y(), coord_step*2.0, coord_step*2.0,
                QPen(), QBrush(Qt::white,Qt::SolidPattern)), pt);

    } else {
        emit led_selected(led_id);
    }
}

//Use this event to initiate LED movement
void design_scene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent * mouseEvent)
{
    QPointF pt = get_snap_coords(QPointF(mouseEvent->scenePos().x(), mouseEvent->scenePos().y()));
    qint8 led_id = led_at_pos(pt);
    if(led_id >= 0) {
        for(int i = 0 ; i < views().length(); i++) {
            views()[i]->viewport()->setCursor(Qt::ClosedHandCursor);
        }
    }
    repos_event = true;
    repos_led_id = led_id;
}

void design_scene::mouseReleaseEvent(QGraphicsSceneMouseEvent * mouseEvent)
{
    (void)mouseEvent;
    for(int i = 0 ; i < views().length(); i++) {
        views()[i]->viewport()->unsetCursor();
    }
    repos_event = false;
}

//Use this event to move LED around in the Scene after mouse is double clicked
void design_scene::mouseMoveEvent(QGraphicsSceneMouseEvent * mouseEvent)
{
    if(repos_event) {
        QPointF pt = get_snap_coords(QPointF(mouseEvent->scenePos().x(), mouseEvent->scenePos().y()));
        set_led_pos(repos_led_id, pt);
        update(sceneRect());
        for(uint i = 0 ; i < views().length(); i++) {
            views()[i]->viewport()->setCursor(Qt::ClosedHandCursor);
        }
        qDebug()<<"Pos " << pt;
    }
}

QPointF design_scene::get_snap_coords(QPointF pt)
{
    return QPointF(roundf((pt.x() - coord_step)/(coord_step*2))*(coord_step*2),
                   roundf((pt.y() - coord_step)/(coord_step*2))*(coord_step*2));
}

qint32 design_scene::add_led(QGraphicsEllipseItem *led, QPointF loc)
{
    led->setFlag(QGraphicsEllipseItem::ItemIsMovable, true);
    QGraphicsSimpleTextItem* id_name = this->addSimpleText(QString::number(num_leds));
    id_name->setBrush(Qt::black);
    id_name->setParentItem(led);
    id_name->setPos(loc.x() + 2.5, loc.y() + 2.5);
    strip.append(led_instance(led,loc,id_name));
    num_leds = strip.length();
    qDebug() << "Created LED ID" << num_leds - 1 << " @ " << loc.x() << " " << loc.y();
    return num_leds;
}

qint8 design_scene::led_at_pos(QPointF loc)
{
    for(int i = 0; i < strip.length(); i++) {
        if(strip[i].loc == loc) {
            return i;
        }
    }
    return -1;
}

QGraphicsEllipseItem* design_scene::get_led_byid(qint8 led_id)
{
    return strip[led_id].led;
}

void design_scene::set_led_pos(uint8_t led_id, QPointF loc)
{
    if(strip.length() <= 0) {
        return;
    }
    for(int i = 0; i < strip.length(); i++) {
        if(strip[i].loc == loc) {
            return;
        }
    }
    qDebug() << "REPOS " << loc;
    QPointF offset =  loc - strip[led_id].loc;
    strip[led_id].led->moveBy(offset.x(), offset.y());
    strip[led_id].loc = loc;
}

void design_scene::push_led_pattern(qint8 led_id, pattern patt)
{
    strip[led_id].pattern_list.append(patt);
}

void design_scene::remove_led_pattern(qint8 selected_led_id, qint16 remove_idx)
{
    strip[selected_led_id].pattern_list.removeAt(remove_idx);
}

void design_scene::delete_led(qint8 selected_led_id)
{
    if(selected_led_id >= strip.length()) {
        return;
    }
    QGraphicsEllipseItem* led;
    led = strip[selected_led_id].led;
    strip.removeAt(selected_led_id);
    delete led;
    for(int8_t i = selected_led_id; i < strip.length(); i++) {
        strip[i].id->setText(QString::number(i));
    }
    num_leds--;
}

QColor design_scene::get_color_at_time(qint8 led_id, qint16 time)
{
    quint16 completed_time = 0;
    quint8 step = 0;
    QColor ret = Qt::black;
    for(quint16 i = 0; i < strip[led_id].pattern_list.length(); i++)
    {
        completed_time += strip[led_id].pattern_list[i].offset;
        if(time < completed_time) {
            return Qt::black;
        }
        completed_time += strip[led_id].pattern_list[i].total_time;
        if(time < completed_time) {
            if(strip[led_id].pattern_list[i].is_solid) {
                //we are in scheduled solid color, send it
                return strip[led_id].pattern_list[i].start_color;
            } else {
                //calculate fraction of pattern time completed
                float t_frac = float(completed_time - time)/strip[led_id].pattern_list[i].total_time;
                QColor s_color = strip[led_id].pattern_list[i].start_color;
                QColor e_color = strip[led_id].pattern_list[i].end_color;
                float mid = strip[led_id].pattern_list[i].mid/100.0; //convert mid to fraction
                //grow start color towards mid and decay again
                if(t_frac <= mid && mid != 0.0) {
                    t_frac /= mid;
                } else {
                    t_frac = (1.0 - t_frac)/(1.0 - mid);
                }
                //do color mixing as per calculated intensities for each color, this created smooth transitions
                return QColor(t_frac*e_color.red() + (1.0-t_frac) * s_color.red(),
                              t_frac*e_color.green() + (1.0-t_frac) * s_color.green(),
                              t_frac*e_color.blue() + (1.0-t_frac) * s_color.blue());
            }
        }
    }
    return ret;
}

void design_scene::save_patterns_to_file(QString& file_name)
{
    qint16 time_stamp = 0;
    QByteArray data;
    QColor curr_color;
    QFile file(file_name, this);
    QColor* prev_color_list;
    prev_color_list = new QColor[strip.length()];
    for(uint8_t i = 0; i < strip.length(); i++) {
        prev_color_list[i] = Qt::black;
    }
    file.open(QIODevice::WriteOnly);
    while(time_stamp < global_loop_time*100 - 1) {
        for(uint8_t i = 0; i < strip.length(); i++) {
            curr_color = get_color_at_time(i, time_stamp);
            if(prev_color_list[i] != curr_color) {
                qDebug() << cnt << " " << curr_color.name();
                data.append(uint8_t(time_stamp >> 8));
                data.append(uint8_t(time_stamp & 0xFF));
                data.append(i);
                data.append(uint8_t(curr_color.red()));
                data.append(uint8_t(curr_color.green()));
                data.append(uint8_t(curr_color.blue()));
                prev_color_list[i] = curr_color;
            }
        }
        time_stamp++;
    }
    //reset all the LEDs at the end of the loop
    for(uint8_t i = 0; i < strip.length(); i++) {
        data.append(uint8_t(time_stamp >> 8));
        data.append(uint8_t(time_stamp & 0xFF));
        data.append(i);
        unsigned char black = 0;
        data.append(black);
        data.append(black);
        data.append(black);
    }
    file.write(data);
    file.close();
    delete[] prev_color_list;
}

void design_scene::loop_player()
{
    QColor curr_color;
    QList<QGraphicsItem*> child_list;
    for(qint8 i = 0; i < strip.length(); i++) {
        curr_color = get_color_at_time(i, cnt);
        if(strip[i].led->brush().color() != curr_color) {
            qDebug() << cnt << i << curr_color.name();
            strip[i].led->setBrush(QBrush(curr_color));
            child_list = strip[i].led->childItems();
            strip[i].id->setBrush(QColor(255 - curr_color.red(),
                                           255 - curr_color.green(),
                                           255 - curr_color.blue()));
        }
    }
    cnt++;
    if(cnt > (global_loop_time*100 - 1)) {
        cnt = 0;    //restart loop
    }
}

void design_scene::reset_design_scene()
{
    if(strip.isEmpty()) {
        return;
    }
    QGraphicsEllipseItem* temp_led;
    for(int i = 0; i < strip.length(); i++)
    {
        temp_led = strip[i].led;
        delete temp_led;
    }
    strip.clear();
    num_leds = 0;
}

void design_scene::load_ledproj(QString fileName)
{
    QFile file(fileName, this);
    if(!file.open(QIODevice::ReadOnly)) {
        qWarning("Couldn't open save file.");
        return;
    }
    reset_design_scene();
    QByteArray data = file.readAll();
    QJsonArray leds;
    QJsonDocument load_doc(QJsonDocument::fromJson(data));
    leds = load_doc.array();
    QJsonObject led;
    for(int i = 0; i < leds.size(); i++) {
        QJsonObject pos;
        QJsonArray pattern_list;
        led = leds[i].toObject();
        pos = led["pos"].toObject();
        pattern_list = led["patterns"].toArray();
        qreal pos_x = pos["locX"].toDouble();
        qreal pos_y = pos["locY"].toDouble();
        QPointF pt = get_snap_coords(QPointF(pos_x, pos_y));
        add_led(this->addEllipse(pt.x(), pt.y(), coord_step*2.0, coord_step*2.0,
                QPen(), QBrush(Qt::white,Qt::SolidPattern)), pt);
        for(int j = 0; j < pattern_list.size(); j++) {
            QJsonObject pattern_item;
            pattern_item = pattern_list[j].toObject();
            qint8 total_time = pattern_item["totalTime"].toInt();
            qint8 mid = pattern_item["mid"].toInt();
            qint8 offset = pattern_item["offset"].toInt();
            QColor start_color = QColor(pattern_item["startColor"].toString());
            QColor end_color = QColor(pattern_item["endColor"].toString());
            bool is_solid = pattern_item["isSolid"].toBool();
            push_led_pattern(i, pattern(total_time, mid, offset, start_color, end_color, is_solid));
        }
    }
}

void design_scene::save_ledproj(QString fileName)
{
    QFile file(fileName, this);
    if(!file.open(QIODevice::WriteOnly)) {
        qWarning("Couldn't open save file.");
        return;
    }
    QJsonObject led;
    QJsonArray leds;
    for(qint8 i = 0; i < strip.length(); i++) {
        QJsonObject pos;
        QJsonArray pattern_list;
        pos["locX"] = strip[i].loc.x();
        pos["locY"] = strip[i].loc.y();
        for(int j = 0; j < strip[i].pattern_list.length(); j++) {
            QJsonObject pattern_item;
            pattern_item["totalTime"] = strip[i].pattern_list[j].total_time;
            pattern_item["offset"] = strip[i].pattern_list[j].offset;
            pattern_item["mid"] = strip[i].pattern_list[j].mid;
            pattern_item["startColor"] = strip[i].pattern_list[j].start_color.name();
            pattern_item["endColor"] = strip[i].pattern_list[j].end_color.name();
            pattern_item["isSolid"] = strip[i].pattern_list[j].is_solid;
            pattern_list.append(pattern_item);
        }
        led["id"] = i;
        led["pos"] = pos;
        led["patterns"] = pattern_list;
        leds.append(led);
    }
    QJsonDocument save_data(leds);
    file.write(save_data.toJson());
    file.close();
}
