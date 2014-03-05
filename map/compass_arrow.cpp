#include "compass_arrow.hpp"

#include "framework.hpp"

#include "../anim/controller.hpp"
#include "../anim/task.hpp"

#include "../gui/controller.hpp"

#include "../geometry/any_rect2d.hpp"
#include "../geometry/transformations.hpp"
#include "../graphics/display_list.hpp"

#include "../graphics/display_list.hpp"
#include "../graphics/screen.hpp"
#include "../graphics/pen.hpp"

using namespace graphics;

namespace
{
  class AlfaCompassAnim : public anim::Task
  {
    typedef anim::Task base_t;
  public:
    AlfaCompassAnim(double start, double end, double timeInterval, double timeOffset, Framework * f)
      : m_start(start)
      , m_end(end)
      , m_current(start)
      , m_timeInterval(timeInterval)
      , m_timeOffset(timeOffset)
      , m_f(f)
    {
    }

    bool IsHiding() const
    {
      return m_start > m_end;
    }

    float GetCurrentAlfa() const
    {
      return m_current;
    }

    virtual void OnStart(double ts)
    {
      m_timeStart = ts;
      base_t::OnStart(ts);
      m_f->Invalidate();
    }

    virtual void OnStep(double ts)
    {
      base_t::OnStep(ts);
      double elapsed = ts - (m_timeStart + m_timeOffset);
      if (elapsed >= 0.0)
      {
        double t = elapsed / m_timeInterval;
        if (t > 1.0)
        {
          m_current = m_end;
          End();
        }
        else
          m_current = m_start + t * (m_end - m_start);
      }

      m_f->Invalidate();
    }

  private:
    double m_start;
    double m_end;
    double m_current;
    double m_timeInterval;
    double m_timeOffset;
    double m_timeStart;

    Framework * m_f;
  };
}

CompassArrow::Params::Params()
  : m_framework(0)
{}

CompassArrow::CompassArrow(Params const & p)
  : base_t(p),
    m_angle(0),
    m_displayList(NULL),
    m_boundRects(1),
    m_framework(p.m_framework)
{
}

void CompassArrow::AnimateShow()
{
  if (!isVisible() && (m_animTask == NULL || IsHidingAnim()))
  {
    setIsVisible(true);
    CreateAnim(0.1, 1.0, 0.2, 0.0, true);
  }

  if (isVisible() && (m_animTask == NULL || IsHidingAnim()))
    CreateAnim(GetCurrentAlfa(), 1.0, 0.2, 0.0, true);
}

void CompassArrow::AnimateHide()
{
  if (isVisible() && (m_animTask == NULL || !IsHidingAnim()))
    CreateAnim(1.0, 0.0, 0.3, 1.0, false);
}

void CompassArrow::SetAngle(double angle)
{
  m_angle = angle;
  setIsDirtyRect(true);
}

m2::PointD CompassArrow::GetPixelSize() const
{
  Resource const * res = GetCompassResource();
  return m2::PointD(res->m_texRect.SizeX(), res->m_texRect.SizeY());
}

vector<m2::AnyRectD> const & CompassArrow::boundRects() const
{
  if (isDirtyRect())
  {
    Resource const * res = GetCompassResource();
    double halfW = res->m_texRect.SizeX() / 2.0;
    double halfH = res->m_texRect.SizeY() / 2.0;

    m_boundRects[0] = m2::AnyRectD(pivot(),
                                   -math::pi / 2 + m_angle,
                                   m2::RectD(-halfW, -halfH, halfW, halfH));

    setIsDirtyRect(false);
  }

  return m_boundRects;
}

void CompassArrow::draw(graphics::OverlayRenderer * r,
                        math::Matrix<double, 3, 3> const & m) const
{
  if (isVisible())
  {
    checkDirtyLayout();

    UniformsHolder holder;
    float a = GetCurrentAlfa();
    LOG(LINFO, ("Compass alfa = ", a));
    holder.insertValue(ETransparency, a);

    math::Matrix<double, 3, 3> drawM = math::Shift(
                                         math::Rotate(
                                           math::Identity<double, 3>(),
                                           m_angle),
                                         pivot());

    r->drawDisplayList(m_displayList, drawM * m, &holder);
  }
}

void CompassArrow::AlfaAnimEnded(bool isVisible)
{
  setIsVisible(isVisible);
  m_animTask.reset();
}

bool CompassArrow::IsHidingAnim() const
{
  ASSERT(m_animTask != NULL, ());
  AlfaCompassAnim * a = static_cast<AlfaCompassAnim *>(m_animTask.get());
  return a->IsHiding();
}

float CompassArrow::GetCurrentAlfa() const
{
  if (m_animTask)
  {
    AlfaCompassAnim * a = static_cast<AlfaCompassAnim *>(m_animTask.get());
    return a->GetCurrentAlfa();
  }

  return 1.0;
}

void CompassArrow::CreateAnim(double startAlfa, double endAlfa, double timeInterval, double timeOffset, bool isVisibleAtEnd)
{
  if (m_framework->GetAnimController() == NULL)
    return;

  if (m_animTask)
    m_animTask->Cancel();
  m_animTask.reset(new AlfaCompassAnim(startAlfa, endAlfa, timeInterval, timeOffset, m_framework));
  m_animTask->AddCallback(anim::Task::EEnded, bind(&CompassArrow::AlfaAnimEnded, this, isVisibleAtEnd));
  m_framework->GetAnimController()->AddTask(m_animTask);
}

const Resource * CompassArrow::GetCompassResource() const
{
  Screen * cacheScreen = m_controller->GetCacheScreen();
  Icon::Info icon("compass-image");
  Resource const * res = m_controller->GetCacheScreen()->fromID(cacheScreen->findInfo(icon));
  ASSERT(res, ("Commpass-image not founded"));
  return res;
}

void CompassArrow::cache()
{
  graphics::Screen * cacheScreen = m_controller->GetCacheScreen();

  purge();
  m_displayList = cacheScreen->createDisplayList();

  cacheScreen->beginFrame();
  cacheScreen->setDisplayList(m_displayList);
  cacheScreen->applyVarAlfaStates();

  Resource const * res = GetCompassResource();
  shared_ptr<gl::BaseTexture> texture = cacheScreen->pipeline(res->m_pipelineID).texture();
  m2::RectU rect = res->m_texRect;
  double halfW = rect.SizeX() / 2.0;
  double halfH = rect.SizeY() / 2.0;

  m2::PointD coords[] =
  {
    m2::PointD(-halfW, -halfH),
    m2::PointD(-halfW, halfH),
    m2::PointD(halfW, -halfH),
    m2::PointD(halfW, halfH),
  };

  m2::PointF normal(0.0, 0.0);
  m2::PointF texCoords[] =
  {
    texture->mapPixel(m2::PointF(rect.minX(), rect.minY())),
    texture->mapPixel(m2::PointF(rect.minX(), rect.maxY())),
    texture->mapPixel(m2::PointF(rect.maxX(), rect.minY())),
    texture->mapPixel(m2::PointF(rect.maxX(), rect.maxY())),
  };

  cacheScreen->addTexturedStripStrided(coords, sizeof(m2::PointD),
                                       &normal, 0,
                                       texCoords, sizeof(m2::PointF),
                                       4, depth(), res->m_pipelineID);


  cacheScreen->setDisplayList(0);
  cacheScreen->endFrame();
}

void CompassArrow::purge()
{
  delete m_displayList;
  m_displayList = NULL;
}

bool CompassArrow::onTapEnded(m2::PointD const & pt)
{
  anim::Controller * animController = m_framework->GetAnimController();
  anim::Controller::Guard guard(animController);

  /// switching off compass follow mode
  m_framework->GetInformationDisplay().locationState()->StopCompassFollowing();

  double startAngle = m_framework->GetNavigator().Screen().GetAngle();
  double endAngle = 0;

  m_framework->GetAnimator().RotateScreen(startAngle, endAngle);

  m_framework->Invalidate();

  return true;
}

bool CompassArrow::roughHitTest(m2::PointD const & pt) const
{
  return hitTest(pt);
}

bool CompassArrow::hitTest(m2::PointD const & pt) const
{
  Resource const * res = GetCompassResource();
  double rad = 1.5 * max(res->m_texRect.SizeX() / 2.0, res->m_texRect.SizeY() / 2.0);
  return pt.Length(pivot()) < rad * visualScale();
}
