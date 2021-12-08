// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#ifndef SPECTMORPH_SCROLLBAR_HH
#define SPECTMORPH_SCROLLBAR_HH

#include "smdrawutils.hh"
#include "smmath.hh"

namespace SpectMorph
{

enum class
Orientation
{
  HORIZONTAL,
  VERTICAL
};

class ScrollBar : public Widget
{
  double m_page_size;
  double m_pos;
  bool   m_center_zoom = false;
  double old_pos;
  double mouse_y;
  double mouse_x;
  bool mouse_down = false;
  bool highlight = false;
  Rect clickable_rect;
  double scroll_factor = 0.25;

  Orientation orientation;

public:
  Signal<double> signal_position_changed;

  ScrollBar (Widget *parent, double page_size, Orientation orientation) :
    Widget (parent),
    m_pos (0),
    orientation (orientation)
  {
    m_page_size = sm_bound<double> (0, page_size, 1);
  }
  void
  draw (const DrawEvent& devent) override
  {
    DrawUtils du (devent.cr);

    const double space = 2;

    const double rwidth = width() - 2 * space;
    const double rheight = height() - 2 * space;

    Color bg_color;
    if (enabled())
      bg_color.set_rgb (0.5, 0.5, 0.5);
    else
      bg_color.set_rgb (0.3, 0.3, 0.3);
    du.round_box (space, space, rwidth, rheight, 1, 5, Color::null(), bg_color);

    Color fg_color;
    if (enabled())
      {
        if (highlight || mouse_down)
          fg_color.set_rgb (0.8, 0.8, 0.8);
        else
          fg_color.set_rgb (0.7, 0.7, 0.7);
      }
    else
      fg_color.set_rgb (0.5, 0.5, 0.5);

    if (orientation == Orientation::HORIZONTAL)
      clickable_rect = Rect (space + m_pos * rwidth, space, rwidth * m_page_size, rheight);
    else
      clickable_rect = Rect (space, space + m_pos * rheight, rwidth, rheight * m_page_size);

    du.round_box (clickable_rect, 1, 5, Color::null(), fg_color);
  }
  void
  mouse_press (const MouseEvent& event) override
  {
    if (event.button != LEFT_BUTTON)
      return;

    if (clickable_rect.contains (event.x, event.y)) /* drag scroll bar */
      {
        mouse_down = true;
        mouse_y = event.y;
        mouse_x = event.x;
        old_pos = m_pos;
        update();
      }
    else /* click, not in scroll bar rect => jump one page */
      {
        double new_pos = m_pos;

        if (orientation == Orientation::HORIZONTAL)
          {
            if (event.x < clickable_rect.x())
              new_pos = m_pos - m_page_size;
            else if (event.x > clickable_rect.x() + clickable_rect.width())
              new_pos = m_pos + m_page_size;
          }
        else
          {
            if (event.y < clickable_rect.y())
              new_pos = m_pos - m_page_size;
            else if (event.y > clickable_rect.y() + clickable_rect.height())
              new_pos = m_pos + m_page_size;
          }

        new_pos = sm_bound (0.0, new_pos, 1 - m_page_size);
        if (m_pos != new_pos)
          {
            m_pos = new_pos;
            signal_position_changed (m_pos);
            update();
          }
      }
  }
  void
  mouse_move (const MouseEvent& event) override
  {
    bool new_highlight = clickable_rect.contains (event.x, event.y);
    if (highlight != new_highlight)
      {
        highlight = new_highlight;
        update();
      }

    if (mouse_down)
      {
        if (orientation == Orientation::VERTICAL)
          m_pos = old_pos + (event.y - mouse_y) / height();
        else
          m_pos = old_pos + (event.x - mouse_x) / width();

        m_pos = sm_bound (0.0, m_pos, 1 - m_page_size);
        signal_position_changed (m_pos);
        update();
      }
  }
  void
  set_scroll_factor (double new_scroll_factor)
  {
    scroll_factor = new_scroll_factor;
  }
  bool
  scroll (double dx, double dy) override
  {
    m_pos = sm_bound<double> (0, m_pos - scroll_factor * m_page_size * dy, 1 - m_page_size);

    signal_position_changed (m_pos);
    update();

    return true;
  }
  void
  mouse_release (const MouseEvent& event) override
  {
    if (event.button == LEFT_BUTTON)
      {
        mouse_down = false;
        update();
      }
  }
  void
  leave_event() override
  {
    highlight = false;
    update();
  }
  double
  pos() const
  {
    return m_pos;
  }
  void
  set_pos (double pos)
  {
    if (pos == m_pos)
      return;

    m_pos = pos;
    update();
  }
  void
  set_page_size (double new_page_size)
  {
    if (m_page_size == new_page_size)
      return;

    new_page_size = sm_bound<double> (0, new_page_size, 1);

    if (m_center_zoom)
      {
        m_pos += m_page_size / 2 - new_page_size / 2;
      }
    else
      {
        m_pos = m_pos / m_page_size * new_page_size;
      }
    m_pos = sm_bound<double> (0, m_pos, 1 - new_page_size);

    m_page_size = new_page_size;

    signal_position_changed (m_pos);
    update();
  }
  double
  page_size() const
  {
    return m_page_size;
  }
  void
  set_center_zoom (bool czoom)
  {
    m_center_zoom = czoom;
  }
};

}

#endif

