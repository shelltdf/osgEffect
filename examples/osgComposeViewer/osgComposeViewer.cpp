
/*

*/

#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgViewer/Viewer>
#include <osgGA/StateSetManipulator>
#include <osgGA/TrackballManipulator>
#include <osgViewer/ViewerEventHandlers>
#include <osg/MatrixTransform>
#include <osgUtil/Optimizer>

#include <osgEFT/EffectManager>
#include <osgEFT/NFX>


osg::Program* loadProgram(const std::string& vs_string, const std::string& fs_string)
{
    osg::Program* p = new osg::Program();
    osg::Shader* vs = new osg::Shader(osg::Shader::Type::VERTEX, vs_string);
    osg::Shader* fs = new osg::Shader(osg::Shader::Type::FRAGMENT, fs_string);
    p->addShader(vs);
    p->addShader(fs);
    return p;
}


osgEFT::PassUpdater_lightspace* newShadowPassAndTarget(osgEFT::EffectManager* em, osg::Light* light, osg::Camera* camera
    , float _near, float _far, int shadowmap_size
    , std::string slot_name, std::string target_name
    , int shadow_unit)
{
    int pid = em->addPass(new osg::Camera(), false
        , osg::Camera::PRE_RENDER
        , osgEFT::Coord(0.0, shadowmap_size), osgEFT::Coord(0.0, shadowmap_size));
    em->setInput(pid, slot_name);

    osgEFT::Pass* pass = em->getPass(pid);
    osgEFT::PassUpdater_lightspace* pu = new osgEFT::PassUpdater_lightspace();
    pu->in_camera = camera;
    pu->in_light = light;
    pu->in_near = _near;
    pu->in_far = _far;
    pu->in_shadow_unit = shadow_unit;
    pass->setPassUpdater(pu);
    em->setPassUpdater(pu);

    em->newTarget(target_name, osgEFT::DT_DEPTH);
    em->setOutput(pid, osg::Camera::BufferComponent::DEPTH_BUFFER, target_name);

    return pu;
}


//debug pass
void addDebugPass(osgEFT::EffectManager* em
    , std::string target_name
    , osgEFT::Coord w, osgEFT::Coord h
    , osgEFT::Coord x, osgEFT::Coord y
)
{
    //isolation shader
    static osg::Program* program = loadProgram(
        "varying vec4 TextureCoord0;\n"
        "void main()\n"
        "{\n"
        "   TextureCoord0 = gl_TextureMatrix[0] * gl_MultiTexCoord0;\n"
        "   gl_Position = ftransform();\n"
        "}\n"
        "\n"
        ,
        "varying vec4 TextureCoord0;\n"
        "uniform sampler2D tex_color;\n"
        "void main()\n"
        "{\n"
        "   gl_FragColor = texture2D(tex_color,TextureCoord0.st);\n"
        "}\n"
        "\n"
    );

    static osg::PolygonMode* pm = new osg::PolygonMode(osg::PolygonMode::Face::FRONT_AND_BACK
        , osg::PolygonMode::Mode::FILL);


    int pid = em->addPass(new osg::Camera(), true, osg::Camera::POST_RENDER, w, h, x, y);
    em->setInput(pid, 0, target_name);
    em->getPassStateSet(pid)->setAttributeAndModes(program, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);
    em->getPassStateSet(pid)->setAttributeAndModes(pm, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);

}

class CullUpdater
    :public osg::NodeCallback
{
public:
    CullUpdater(osgEFT::EffectManager* _em)
        :em(_em) {}
    virtual ~CullUpdater() {}
    virtual bool run(osg::Object* object, osg::Object* data)
    {
        em->update();
        return traverse(object, data);
    }
    osgEFT::EffectManager* em;
};


void enableEffect(
    std::string filename
    , osgEFT::EffectManager* em
    , osg::Group* scene
    //, osg::Group* scene
    , osg::Camera* camera
    //, osg::Light* light
)
{
    printf("enable effect\n");


#if 1
    
    osg::ref_ptr<osgEFT::NFX> nfx = new osgEFT::NFX();
    if (nfx->load(filename))
    {
        nfx->applyCompose(em, scene, camera);
    }

#else

    //cleanup
    em->cleanUp();

    //source
    osg::Node* node = osgDB::readNodeFile("../data/tire.obj");
    em->addSlot("default", node);

    //pass
    int pid = em->addPassFromMainCamera(camera, scene, osgEFT::Coord(1.0, 0), osgEFT::Coord(1.0, 0));
    em->setInput(pid, "default");


    //
    osg::Texture2D* tex = new osg::Texture2D();
    //tex->setResizeNonPowerOfTwoHint(false);
    //tex->setTextureSize(512, 512);
    tex->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR);
    tex->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
    tex->setInternalFormat(GL_RGBA);
    tex->setSourceFormat(GL_RGBA);


    //buffer
    //em->newTarget("color_map", osgEFT::DT_RGBA);
    em->newTarget("color_map", tex);
    em->setOutput(pid, osg::Camera::BufferComponent::COLOR_BUFFER0, "color_map", 4);

    //
    pid = em->addPass(new osg::Camera(), true, osg::Camera::POST_RENDER
        , osgEFT::Coord(0.0, 256), osgEFT::Coord(0.0, 256)
        , osgEFT::Coord(0.0, 256), osgEFT::Coord(0.0, 0)
    );
    em->setInput(pid, 0, "color_map");

    //shader
    //em->getPassStateSet(pid)->setAttributeAndModes(
    //    loadProgram(
    //        "varying vec4 TextureCoord0;\n"
    //        "void main()\n"
    //        "{\n"
    //        "   TextureCoord0 = gl_TextureMatrix[0] * gl_MultiTexCoord0;\n"
    //        "   gl_Position = ftransform();\n"
    //        "}\n"
    //        "\n"
    //        ,
    //        "varying vec4 TextureCoord0;\n"
    //        "uniform sampler2D tex_color;\n"
    //        "void main()\n"
    //        "{\n"
    //        "   vec4 color = texture2D(tex_color,TextureCoord0.st);\n"
    //        "   gl_FragColor = vec4(color.r,0,0,1);\n"
    //        "}\n"
    //        "\n"
    //    )
    //    , osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);
#endif // 0

    //m_enable = false;
}



int main(int argc, char** argv)
{
    //input file
    std::string filename = "../data/compose2.xml";

    //create viewer
    osgViewer::Viewer viewer;
    viewer.setThreadingModel(osgViewer::Viewer::ThreadingModel::SingleThreaded);
    viewer.getCamera()->setClearColor(osg::Vec4(0.2, 0.2, 0.2, 1));
    viewer.getLight()->setPosition(osg::Vec4(1, 1, 1, 0));

    /*
    * root
    *    scene
    *    effect_manager
    */

    //root
    osg::ref_ptr<osg::Group> root = new osg::Group;
    root->setName("root");
    viewer.setSceneData(root);

    //neighbour of effect manager
    osg::ref_ptr<osg::Group> scene = new osg::Group;
    scene->setName("scene");
    root->addChild(scene);

    //effect manager
    osgEFT::EffectManager* em = new osgEFT::EffectManager();
    em->setName("effect manager");
    root->addChild(em);
    viewer.getCamera()->addCullCallback(new CullUpdater(em));

    //
    enableEffect(filename, em, scene, viewer.getCamera()/*, viewer.getLight()*/);
    //viewer.addEventHandler(new EffectScript(root, scene_root, scene, viewer.getLight(), viewer.getCamera()));

    //debug
    osgDB::writeNodeFile(*em,"root.osg");

    //
    viewer.addEventHandler(new osgGA::StateSetManipulator(viewer.getCamera()->getOrCreateStateSet()));
    viewer.addEventHandler(new osgViewer::ThreadingHandler);
    viewer.addEventHandler(new osgViewer::WindowSizeHandler);
    viewer.addEventHandler(new osgViewer::StatsHandler);
    viewer.addEventHandler(new osgViewer::RecordCameraPathHandler);
    viewer.addEventHandler(new osgViewer::LODScaleHandler);
    viewer.addEventHandler(new osgViewer::ScreenCaptureHandler);


    //realize
    //viewer.setUpViewOnSingleScreen();
    viewer.setUpViewInWindow(100, 100, 1280, 720);
    viewer.frame();
    osgViewer::GraphicsWindow* gw = dynamic_cast<osgViewer::GraphicsWindow*>(viewer.getCamera()->getGraphicsContext());
    if (gw)
    {
        // Send window size event for initializing
        int x, y, w, h; gw->getWindowRectangle(x, y, w, h);
        viewer.getEventQueue()->windowResize(x, y, w, h);
    }

    //run
    viewer.run();

    return 0;
}

