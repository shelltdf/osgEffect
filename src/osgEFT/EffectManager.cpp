
#include<osgEFT/EffectManager>



using namespace osgEFT;


//处理resize消息
class EffectManagerEventHandler
    :public osgGA::GUIEventHandler
{
public:
    EffectManagerEventHandler() {}
    virtual ~EffectManagerEventHandler() {}

    virtual bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa, osg::Object* obj, osg::NodeVisitor* nv) override
    {
        if (ea.getEventType() == osgGA::GUIEventAdapter::RESIZE)
        {
            osgEFT::EffectManager* em = dynamic_cast<osgEFT::EffectManager*>(obj);
            if (em)
            {
                em->resize(ea.getWindowWidth(), ea.getWindowHeight());
            }
        }
        return osgGA::GUIEventHandler::handle(ea, aa, obj, nv);
    }
};



EffectManager::EffectManager()
{
    this->setEventCallback(new EffectManagerEventHandler());
}

EffectManager::~EffectManager()
{

}

void EffectManager::newTarget(std::string name, osg::Texture* texture)
{
    mTargets[name] = new Target();
    mTargets[name]->texture = texture;
}

void EffectManager::newTarget(std::string name, TARGET_TYPE type)
{
    mTargets[name] = new Target();
    
    osg::Texture2D* tex = new osg::Texture2D();
    tex->setResizeNonPowerOfTwoHint(false);
    tex->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR);
    tex->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
    //tex->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::NEAREST);
    //tex->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::NEAREST);
    mTargets[name]->texture = tex;
    //mTargets[name]->coord_w = w;
    //mTargets[name]->coord_h = h;

    tex->setTextureSize(512, 512); //default
    tex->dirtyTextureObject();

    //GL_ALPHA

    switch (type)
    {
    case osgEFT::DT_LUMINANCE:
        tex->setInternalFormat(GL_LUMINANCE);
        tex->setSourceFormat(GL_LUMINANCE);
        //tex->setSourceType(GL_FLOAT);
        break;
    case osgEFT::DT_RGB:
        tex->setInternalFormat(GL_RGB);
        tex->setSourceFormat(GL_RGB);
        //tex->setSourceType(GL_UNSIGNED_BYTE);
        break;
    case osgEFT::DT_RGBA:
        tex->setInternalFormat(GL_RGBA);
        tex->setSourceFormat(GL_RGBA);
        //tex->setSourceType(GL_UNSIGNED_BYTE);
        break;
    case osgEFT::DT_DEPTH:
        tex->setInternalFormat(GL_DEPTH_COMPONENT24);
        tex->setSourceFormat(GL_DEPTH_COMPONENT);
        //tex->setSourceType(GL_FLOAT);

        //tex->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::NEAREST);
        //tex->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::NEAREST);
        tex->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP);
        tex->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP);

        //glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);

        // This is to allow usage of shadow2DProj function in the shader
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        //glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);

        //tex->setShadowCompareFunc(osg::Texture2D::ShadowCompareFunc::LEQUAL);
        //tex->setShadowComparison(true);
        //tex->setShadowTextureMode(osg::Texture2D::ShadowTextureMode::INTENSITY);

        break;
    default:
        break;
    }

}

void EffectManager::newTarget(std::string name, GLint internal_format, GLenum data_type, GLenum format)
{
    mTargets[name] = new Target();

    osg::Texture2D* tex = new osg::Texture2D();
    tex->setResizeNonPowerOfTwoHint(false);
    tex->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR);
    tex->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
    //tex->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::NEAREST);
    //tex->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::NEAREST);
    mTargets[name]->texture = tex;

    tex->setTextureSize(512, 512); //default
    //tex->dirtyTextureObject();

    //GL_ALPHA
    tex->setInternalFormat(internal_format);
    tex->setSourceFormat(format);
    tex->setSourceType(data_type);
}

void EffectManager::newTargetImage(std::string name, TARGET_TYPE type, int w, int h)
{
    mTargets[name] = new Target();

    osg::Image* image = new osg::Image();
    mTargets[name]->image = image;

    switch (type)
    {
    case osgEFT::DT_RGB:
        image->allocateImage(w, h, 1, GL_RGB, GL_UNSIGNED_BYTE);
        break;
    case osgEFT::DT_RGBA:
        image->allocateImage(w, h, 1, GL_RGBA, GL_UNSIGNED_BYTE);
        break;
    case osgEFT::DT_DEPTH:
        image->allocateImage(w, h, 1, GL_DEPTH_COMPONENT,GL_UNSIGNED_SHORT);
        break;
    default:
        break;
    }

}

void EffectManager::newTargetImage(std::string name, osg::Image* image)
{
    mTargets[name] = new Target();
    mTargets[name]->image = image;
}

int EffectManager::addPass(osg::Camera* camera, bool render_rect
    , osg::Camera::RenderOrder render_order
    , Coord w, Coord h
    , Coord x, Coord y )
{
    Pass* pass = new Pass();
    pass->camera = camera;
    pass->coord_x = x;
    pass->coord_y = y;
    pass->coord_w = w;
    pass->coord_h = h;
    mPasses.push_back(pass);
#if USE_CAMERA_CHILD
    this->addChild(camera);
#endif

    camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //camera->setClearColor(osg::Vec4(0.5, 0.5, 0.5, 1));
    //camera->setClearColor(osg::Vec4(1.0, 0.5, 1.0, 1));
    camera->setViewport(pass->coord_x.get(512), pass->coord_y.get(512)
        , pass->coord_w.get(512), pass->coord_h.get(512)); //default
    //camera->setRenderOrder(osg::Camera::RenderOrder::PRE_RENDER, 0);
    camera->setRenderOrder(render_order, 0);
    
    //camera->getOrCreateStateSet()->setRenderBinMode(osg::StateSet::RenderBinMode::OVERRIDE_PROTECTED_RENDERBIN_DETAILS);
    //camera->getOrCreateStateSet()->setRenderBinDetails(1, "RenderBin");

    if (render_rect)
    {
        osg::Geometry* geom = osg::createTexturedQuadGeometry(osg::Vec3(-1, -1, 0), osg::Vec3(2, 0, 0), osg::Vec3(0, 2, 0));
        osg::Geode* geode = new osg::Geode();
        geode->addDrawable(geom);
        camera->addChild(geode);

        camera->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
        camera->setComputeNearFarMode(osg::Camera::DO_NOT_COMPUTE_NEAR_FAR);
        //pass->setProjectionMatrixAsOrtho(-0.1, 1.1, -0.1, 1.1, 0, 1);
        camera->setProjectionMatrixAsOrtho(-1, 1, -1, 1, 0, 1);
        //camera->setProjectionMatrixAsOrtho(-1.01, 1.01, -1.01, 1.01, 0, 1);
        camera->setViewMatrixAsLookAt(osg::Vec3(0, 0, 0.999999), osg::Vec3(0, 0, 0), osg::Vec3(0, 1, 0));
        camera->getOrCreateStateSet()->setMode(GL_LIGHTING, false);
        //pass->setViewport(0, 0, 1, 1);
    }

    return mPasses.size() - 1;
}

int EffectManager::addPassFromMainCamera(osg::Camera* camera, osg::Group* camera_scene_root, Coord w, Coord h)
{
    Pass* pass = new Pass();
    pass->camera = camera;
    pass->coord_w = w;
    pass->coord_h = h;
    pass->is_main_camera = true;
    pass->main_camera_scene_root = camera_scene_root;
    mPasses.push_back(pass);

    camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //camera->setClearColor(osg::Vec4(1, 0, 1, 1));
    //camera->setRenderOrder(osg::Camera::RenderOrder::POST_RENDER, 0);

    return mPasses.size() - 1;
}

#if 0
int EffectManager::addPassFromMainCamera2(osg::View* view, osg::Camera* _camera, Coord w, Coord h)
{
    osg::Camera* camera = new osg::Camera();
    view->addSlave(camera);
    camera->setName("slave camera");
    //this->addChild(camera);
    camera->setGraphicsContext(_camera->getGraphicsContext());
    camera->setRenderOrder(osg::Camera::RenderOrder::POST_RENDER, 1);

    Pass* pass = new Pass();
    pass->camera = camera;
    pass->coord_w = w;
    pass->coord_h = h;
    //pass->is_main_camera = true;
    //pass->main_camera_scene_root = camera_scene_root;
    mPasses.push_back(pass);

    camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    camera->setClearColor(osg::Vec4(1, 0, 0, 1));

    return mPasses.size() - 1;
}
#endif

osg::StateSet* EffectManager::getPassStateSet(int i)
{ 
    Pass* pass = getPass(i);
    if (pass)
    {
        if (pass->is_main_camera)
        {
            return pass->main_camera_scene_root->getOrCreateStateSet();
        }
        else
        {
            return pass->camera->getOrCreateStateSet();
        }
    }
    return 0; 
}

void EffectManager::setInput(int pass_id, std::string node_name, unsigned int cull_mask)
{
    osg::Node* node = getSlot(node_name);
    Pass* pass = getPass(pass_id);
    if (node && pass)
    {
        if (pass->is_main_camera)
        {
            pass->main_camera_scene_root->addChild(node);
        }
        else
        {
            pass->camera->addChild(node);
        }
        pass->camera->setCullMask(cull_mask);
    }
    else
    {
        std::cout << "unknown input node:" << node_name.c_str() << std::endl;
    }
}

void EffectManager::setInput( int pass_id, int unit_id, std::string target_name)
{
    Target* target = getTarget(target_name);
    Pass* pass = getPass(pass_id);
    if (target && pass)
    {
        unsigned int v = osg::StateAttribute::ON | osg::StateAttribute::PROTECTED;
        osg::StateSet* ss = pass->camera->getOrCreateStateSet();
        if (pass->is_main_camera)
        {
            //v = osg::StateAttribute::ON /*| osg::StateAttribute::OVERRIDE*/;
            ss = pass->main_camera_scene_root->getOrCreateStateSet();
        }

        ss->addUniform(new osg::Uniform(target_name.c_str(), int(unit_id)),v);

        if (target->texture.valid())
        {
            ss->setTextureAttributeAndModes(unit_id, target->texture, v);
        }
        else if (target->image.valid())
        {
            osg::Texture2D* tex = new osg::Texture2D(target->image);
            tex->setResizeNonPowerOfTwoHint(false);
            tex->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::NEAREST);
            tex->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::NEAREST);
            ss->setTextureAttributeAndModes(unit_id, tex, v);
        }
    }
    else
    {
        std::cout << "unknown input target:" << target_name.c_str() << std::endl;
    }
}

void EffectManager::setOutput(int pass_id, osg::Camera::BufferComponent component
    , std::string target_name, unsigned int multisampleSamples)
{
    Target* target = getTarget(target_name);
    Pass* pass = getPass(pass_id);
    if (target && pass)
    {
        if (pass->camera->getRenderTargetImplementation() != osg::Camera::FRAME_BUFFER_OBJECT)
        {
            pass->camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        }

        if (target->texture.valid())
        {
            pass->camera->attach(component, target->texture, 0, 0, false, multisampleSamples, multisampleSamples);
            pass->camera->attach(component, target->texture->getInternalFormat());
        }
        else if (target->image.valid())
        {
            pass->camera->attach(component, target->image, multisampleSamples, multisampleSamples);
        }

        //pass->camera->dirtyAttachmentMap();
    }
    else
    {
        std::cout << "unknown output target:" << target_name.c_str() << std::endl;
    }
}

void EffectManager::addDebugPass()
{

}

void EffectManager::resize(float w, float h)
{
    for (size_t i = 0; i < mPasses.size(); i++)
    {
        //
        //printf("w = %f h = %f\n", mPasses[i]->coord_w.get(w), mPasses[i]->coord_h.get(h));

        //更新摄像机视口
        osg::Camera* camera = mPasses[i]->camera;
        camera->setViewport(mPasses[i]->coord_x.get(w), mPasses[i]->coord_y.get(h)
            , mPasses[i]->coord_w.get(w), mPasses[i]->coord_h.get(h));

        //
        if (mPasses[i]->is_main_camera)
        {
            double fovy; double aspectRatio; double zNear; double zFar;
            bool b = camera->getProjectionMatrixAsPerspective(fovy, aspectRatio, zNear, zFar);
            if (b)
            {
                //camera->setProjectionMatrixAsPerspective(fovy, mPasses[i]->coord_w.get(w) / mPasses[i]->coord_h.get(h), zNear, zFar);
                camera->setProjectionMatrixAsPerspective(fovy, (w) / (h), zNear, zFar);
            }
        }

        //更新渲染目标
        osg::Camera::BufferAttachmentMap bam = camera->getBufferAttachmentMap();
        for (auto it = bam.begin(); it != bam.end(); it++)
        {
            osg::Texture* tex = it->second._texture;
            if (tex)
            {
                osg::Texture2D* tex2d = dynamic_cast<osg::Texture2D*>(tex);
                if (tex2d)
                {
                    tex2d->setTextureSize(mPasses[i]->coord_w.get(w), mPasses[i]->coord_h.get(h));
                    tex2d->dirtyTextureObject();
                    tex2d->dirtyTextureParameters();
                }
            }
        }

        //
        camera->dirtyAttachmentMap();
    }

}

