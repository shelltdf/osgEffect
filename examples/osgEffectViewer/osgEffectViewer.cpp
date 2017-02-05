
/*

*/


#include <osgDB/ReadFile>
#include <osgViewer/Viewer>
#include <osgGA/StateSetManipulator>
#include <osgGA/TrackballManipulator>
#include <osgViewer/ViewerEventHandlers>
#include <osg/MatrixTransform>
#include <osgUtil/Optimizer>

#include <osgEFT/EffectManager>


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


class EffectScript
    :public osgGA::GUIEventHandler
{
public:
    EffectScript(osg::Group* root, osg::Group* scene_root, osg::Group* scene, osg::Light* light, osg::Camera* main_camera)
        :m_root(root), m_scene_root(scene_root), m_scene(scene)
        , m_light(light), m_camera(main_camera)
        , m_enable(false)
    {
        //create EffectManager
        m_em = new osgEFT::EffectManager();
        m_root->addChild(m_em);

        //CullUpdater
        main_camera->addCullCallback(new CullUpdater(m_em));

        enableEffect();
    }
    virtual ~EffectScript() {}

    void enableEffect(/*osg::Group* parent, osg::Group* scene, osg::Light* light, osg::Camera* camera*/);
    void disableEffect(/*osg::Group* parent, osg::Group* scene, osg::Light* light, osg::Camera* camera*/);

    virtual bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa)
    {
        osgViewer::Viewer* viewer = dynamic_cast<osgViewer::Viewer*>(&aa);
        if (viewer)
        {
            if (ea.getEventType() == osgGA::GUIEventAdapter::KEYUP)
            {
                if (ea.getKey() == osgGA::GUIEventAdapter::KEY_A)
                {
                    //printf("a");

                    if (m_enable)
                    {
                        disableEffect();
                    }
                    else
                    {
                        enableEffect();
                    }

                    //resize
                    osgViewer::GraphicsWindow* gw = dynamic_cast<osgViewer::GraphicsWindow*>(viewer->getCamera()->getGraphicsContext());
                    if (gw)
                    {
                        int x, y, w, h; gw->getWindowRectangle(x, y, w, h);
                        m_em->resize(w, h);
                    }
                }
            }
        }

        return false;
    }

private:

    osg::ref_ptr<osgEFT::EffectManager> m_em;
    osg::ref_ptr<osg::Group> m_root;
    osg::ref_ptr<osg::Group> m_scene_root;
    osg::ref_ptr<osg::Group> m_scene;
    osg::ref_ptr<osg::Light> m_light;
    osg::ref_ptr<osg::Camera> m_camera;
    bool m_enable;
};


void EffectScript::disableEffect(/*osg::Group* parent, osg::Group* scene, osg::Light* light, osg::Camera* camera*/)
{
    printf("disable effect\n");

    //cleanup
    m_em->cleanUp();

    //source
    m_em->addSlot("default", m_scene);

    //pass
    int pid = m_em->addPassFromMainCamera(m_camera, m_scene_root, osgEFT::Coord(1.0, 0), osgEFT::Coord(1.0, 0));
    m_em->setInput(pid, "default");

    //buffer
    m_em->newTarget("color_map", osgEFT::DT_RGBA);
    m_em->setOutput(pid, osg::Camera::BufferComponent::COLOR_BUFFER0, "color_map", 4);
    pid = m_em->addPass(new osg::Camera(), true, osg::Camera::POST_RENDER, osgEFT::Coord(1.0, 0), osgEFT::Coord(1.0, 0));
    m_em->setInput(pid, 0, "color_map");

    //shader
    m_em->getPassStateSet(pid)->setAttributeAndModes(
        loadProgram(
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
            "   vec4 color = texture2D(tex_color,TextureCoord0.st);\n"
            "   gl_FragColor = vec4(color.r,0,0,1);\n"
            "}\n"
            "\n"
        )
        , osg::StateAttribute::ON | osg::StateAttribute::PROTECTED);

    m_enable = false;
}

void EffectScript::enableEffect(/*osg::Group* parent, osg::Group* scene, osg::Light* light, osg::Camera* camera*/)
{
    printf("enable effect\n");

    //cleanup
    m_em->cleanUp();

    //source
    m_em->addSlot("default", m_scene);


    //
    //shadow pass 0 - 3
    //

    osgEFT::PassUpdater_lightspace* pu0 = newShadowPassAndTarget(m_em, m_light, m_camera
        , 0.001, 15.0
        , 1024
        , "default", "light_depth0"
        , 4);
    osgEFT::PassUpdater_lightspace* pu1 = newShadowPassAndTarget(m_em, m_light, m_camera
        , 15.0, 50.0
        , 1024
        , "default", "light_depth1"
        , 5);
    osgEFT::PassUpdater_lightspace* pu2 = newShadowPassAndTarget(m_em, m_light, m_camera
        , 50.0, 300.0
        , 1024
        , "default", "light_depth2"
        , 6);
    osgEFT::PassUpdater_lightspace* pu3 = newShadowPassAndTarget(m_em, m_light, m_camera
        , 300.0, 2000.0
        , 1024
        , "default", "light_depth3"
        , 7);


    //
    //MRT pass
    //

    //
    int pid = m_em->addPassFromMainCamera(m_camera, m_scene_root, osgEFT::Coord(1.0, 0), osgEFT::Coord(1.0, 0));
    m_em->setInput(pid, "default");

    m_em->setInput(pid, 4, "light_depth0");
    m_em->setInput(pid, 5, "light_depth1");
    m_em->setInput(pid, 6, "light_depth2");
    m_em->setInput(pid, 7, "light_depth3");

    pu0->in_ss = m_em->getPassStateSet(pid);
    pu1->in_ss = m_em->getPassStateSet(pid);
    pu2->in_ss = m_em->getPassStateSet(pid);
    pu3->in_ss = m_em->getPassStateSet(pid);

    m_em->newTarget("color_map", osgEFT::DT_RGBA);
    m_em->setOutput(pid, osg::Camera::BufferComponent::COLOR_BUFFER0, "color_map", 4);
    m_em->newTarget("depth_map", osgEFT::DT_DEPTH);
    m_em->setOutput(pid, osg::Camera::BufferComponent::DEPTH_BUFFER, "depth_map");
    m_em->newTarget("shadow_map", osgEFT::DT_RGB);
    m_em->setOutput(pid, osg::Camera::BufferComponent::COLOR_BUFFER1, "shadow_map");


    //compute near far
    osgEFT::PassUpdater_nearfar* pu = new osgEFT::PassUpdater_nearfar();
    osgEFT::Pass* pass = m_em->getPass(pid);
    pass->setPassUpdater(pu);
    m_em->setPassUpdater(pu);

#if 1
    //MRT:
    //      diffuse
    //      linear depth
    //      shadow
    m_em->getPassStateSet(pid)->setAttributeAndModes(loadProgram(
        "varying vec4 vecInEye;\n"
        "varying vec4 TextureCoord0;\n"

        "uniform mat4 osg_ViewMatrixInverse;\n"
        "varying vec4 ShadowCoord0;\n"
        "varying vec4 ShadowCoord1;\n"
        "varying vec4 ShadowCoord2;\n"
        "varying vec4 ShadowCoord3;\n"
        "const mat4 biasMatrix = mat4(0.5, 0.0, 0.0, 0.0,\n"
        "    0.0, 0.5, 0.0, 0.0, \n"
        "    0.0, 0.0, 0.5, 0.0, \n"
        "    0.5, 0.5, 0.5, 1.0); \n"

        "void main()\n"
        "{\n"
        "   TextureCoord0 = gl_TextureMatrix[0] * gl_MultiTexCoord0;\n"
        "   vecInEye = gl_ModelViewMatrix * gl_Vertex; \n"
        "   gl_Position = ftransform();\n"

        "   vec4 wp = osg_ViewMatrixInverse * gl_ModelViewMatrix * gl_Vertex;\n"
        "   ShadowCoord0 = biasMatrix * gl_TextureMatrix[4] * wp;\n"
        "   ShadowCoord1 = biasMatrix * gl_TextureMatrix[5] * wp;\n"
        "   ShadowCoord2 = biasMatrix * gl_TextureMatrix[6] * wp;\n"
        "   ShadowCoord3 = biasMatrix * gl_TextureMatrix[7] * wp;\n"

        "}\n"
        "\n"
        ,
        "varying vec4 vecInEye;\n"
        "varying vec4 TextureCoord0;\n"
        "uniform vec4 osg_fanf;\n"
        "uniform sampler2D tex_color;\n"

        "uniform sampler2D light_depth0;\n"
        "uniform sampler2D light_depth1;\n"
        "uniform sampler2D light_depth2;\n"
        "uniform sampler2D light_depth3;\n"
        "varying vec4 ShadowCoord0;\n"
        "varying vec4 ShadowCoord1;\n"
        "varying vec4 ShadowCoord2;\n"
        "varying vec4 ShadowCoord3;\n"


        "float compute_shadow( sampler2D light_depth , vec4 ShadowCoord ,vec2 f )\n"
        "{\n"
        "   vec4 shadowCoordinateWdivide = ShadowCoord / ShadowCoord.w;\n"
        "   shadowCoordinateWdivide.z -= 0.003;\n"
        "   float shadow = 1.0;\n"
        "   if (ShadowCoord.w > 0.0)\n"
        "   {\n"
        "       float distanceFromLight = texture2D(light_depth, shadowCoordinateWdivide.st + f * vec2(0.0005,0.0005) ).z;\n"
        "       shadow = distanceFromLight < shadowCoordinateWdivide.z ? 0.0 : 1.0;\n"
        "   }\n"
        "   return shadow;\n"
        "}\n"

        "float compute_shadow_pcf( sampler2D light_depth , vec4 ShadowCoord )\n"
        "{\n"
        "   float shadow = 0.0;\n"
        "       float x=0.0, y=0.0;\n"
        "       for (y = -1.5; y <= 1.5; y += 1.0)\n"
        "           for (x = -1.5; x <= 1.5; x += 1.0)\n"
        "               shadow += compute_shadow( light_depth , ShadowCoord , vec2(x,y) );\n"
        "       shadow /= 16.0;\n"
        "   return shadow;\n"
        "}\n"

        "vec4 pssm(){\n"
        "   vec4 shadow_color = vec4(0.5,0.5,0.5,1);\n"
        "   float eyeDepth = -vecInEye.z;\n"
        "   float shadow = 1.0;\n"
        "   float ShadowDistance0 = 15.0;\n"
        "   float ShadowDistance1 = 50.0;\n"
        "   float ShadowDistance2 = 300.0;\n"
        "   float ShadowDistance3 = 2000.0;\n"
        "   float maxShadowDistance = 2000.0;\n"

        "   if (eyeDepth < maxShadowDistance)\n"
        "   {\n"
        "       if (eyeDepth < ShadowDistance0 && eyeDepth>0.001)\n"
        "       {\n"
        "           shadow = compute_shadow_pcf(light_depth0, ShadowCoord0);\n"
        //"       shadow_color = vec4(1,0,0,1);\n"
        "       }\n"
        "       else if (eyeDepth < ShadowDistance1)\n"
        "       {\n"
        "           shadow = compute_shadow_pcf(light_depth1, ShadowCoord1);\n"
        //"       shadow_color = vec4(0,1,0,1);\n"
        "       }\n"
        "       else if (eyeDepth < ShadowDistance2)\n"
        "       {\n"
        "           shadow = compute_shadow_pcf(light_depth2, ShadowCoord2);\n"
        //"       shadow_color = vec4(0,0,1,1);\n"
        "       }\n"
        "       else if (eyeDepth < ShadowDistance3)\n"
        "       {\n"
        "           shadow = compute_shadow_pcf(light_depth3, ShadowCoord3);\n"
        //"       shadow_color = vec4(1,0,1,1);\n"
        "       }\n"
        "       shadow = clamp(shadow, 0.0, 1.0); \n"
        "}\n"

        //"     gl_FragColor = shadow * gl_Color;\n"
        "       shadow_color = (1.0-shadow) * shadow_color + shadow * vec4(1,1,1,1);\n"
        //"     gl_FragColor = vec4(debug.x*0.1,debug.y*0.1,debug.z*0.1,1);\n"
        "    return shadow_color;\n"
        "}"

        "void main()\n"
        "{\n"
        "   float linDepth = ((-vecInEye.z) - osg_fanf.z)/(osg_fanf.w - osg_fanf.z);// [near, far] -> [0,1]\n"
        "   gl_FragDepth = linDepth ;\n"
        "   gl_FragData[0] = vec4(1,1,1,1) ; // /*gl_FrontMaterial.diffuse +*/ texture2D(tex_color,TextureCoord0.st);\n"
        "   gl_FragData[1] = pssm();\n"
        "}\n"
        "\n"
    ));




    //
    //final pass
    //

    pid = m_em->addPass(new osg::Camera(), true, osg::Camera::POST_RENDER, osgEFT::Coord(1.0, 0), osgEFT::Coord(1.0, 0));
    m_em->setInput(pid, 0, "color_map");
    m_em->setInput(pid, 1, "depth_map");
    m_em->setInput(pid, 2, "shadow_map");
    pu->in_ss = m_em->getPassStateSet(pid);

    m_em->getPassStateSet(pid)->setAttributeAndModes(loadProgram(
        //"uniform mat4 osg_ViewMatrixInverse;\n"
        "varying vec4 TextureCoord0;\n"
        "varying vec4 vecInEye;\n"
        "void main()\n"
        "{\n"
        "   vecInEye = gl_ModelViewMatrix * gl_Vertex; \n"
        "   TextureCoord0 = gl_TextureMatrix[0] * gl_MultiTexCoord0;\n"
        "   gl_Position = ftransform();\n"
        "   gl_FrontColor = gl_Color;\n"
        "}\n"
        "\n"
        ,
        //"uniform mat4 osg_ViewMatrixInverse;\n"
        "uniform sampler2D color_map;\n"
        "uniform sampler2D depth_map;\n"
        "uniform sampler2D shadow_map;\n"
        "uniform vec4 osg_fanf;\n"
        "varying vec4 TextureCoord0;\n"
        "varying vec4 vecInEye;\n"
        //"varying vec4 debug;\n"

        "#define MOD3 vec3(.1031,.11369,.13787)\n"

        "vec2 hash22(vec2 p)\n"
        "{\n"
        "    vec3 p3 = fract(vec3(p.xyx) * MOD3);\n"
        "    p3 += dot(p3, p3.yzx + 19.19);\n"
        "    return fract(vec2((p3.x + p3.y)*p3.z, (p3.x + p3.z)*p3.y));\n"
        " }\n"

        //range 0-1
        "vec2 getRandom(vec2 uv) {\n"
        "   return /*normalize*/( hash22( uv * 126.1231 ) * 2.0 - 1.0);\n"
        "}\n"


        "float ssao( sampler2D in_depth_map , float linear_depth )\n"
        "{\n"
        // sample neighbor pixels
        "   float ao = 0.0;\n"
        // sample zbuffer (in linear eye space) at the current shading point	
        "   float zr = linear_depth;\n"
        "   float x=0.0, y=0.0;\n"
        "   for (y = -1.5; y <= 1.5; y += 1.0)\n"
        "   {\n"
        "       for (x = -1.5; x <= 1.5; x += 1.0)\n"
        "       {\n"
        // get a random 2D offset vector
        "           vec2 off = getRandom( getRandom( TextureCoord0.st ) );\n"
        // sample the zbuffer at a neightbor pixel (in a 16 pixel radious)        		
        "           float z = texture2D(in_depth_map,  TextureCoord0.st + floor(vec2(x,y)*3.5) / vec2(1024,1024)  ).x;\n"
        // accumulate occlusion if difference is less than 0.1 units		
        "           ao += clamp( ((zr - z))/0.01 , 0.0, 1.0);\n"
        "       }\n"
        "   }\n"

        // average down the occlusion	
        "   ao = clamp(1.0 - ao/16.0 , 0.0, 1.0);\n"
        "   return ao;\n"
        "}\n"


        "void main()\n"
        "{\n"
        "    vec4 ao_color = ssao( depth_map , texture2D( depth_map , TextureCoord0.st).x );"
        "    vec4 color =  texture2D(color_map,TextureCoord0.st);\n"
        "    vec4 shadow_color =  texture2D(shadow_map,TextureCoord0.st);\n"
        "    gl_FragColor = color * shadow_color * ao_color;\n"
        "}\n"
        "\n"
    ), osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

#endif


    //debug pass
    if (false)
    {
        //
        int cell = 256;

        //line 1
        addDebugPass(m_em, "color_map"
            , osgEFT::Coord(0.0, cell), osgEFT::Coord(0.0, cell), osgEFT::Coord(0.0, cell * 0), osgEFT::Coord(0.0, cell * 0));
        addDebugPass(m_em, "depth_map"
            , osgEFT::Coord(0.0, cell), osgEFT::Coord(0.0, cell), osgEFT::Coord(0.0, cell * 1), osgEFT::Coord(0.0, cell * 0));
        addDebugPass(m_em, "shadow_map"
            , osgEFT::Coord(0.0, cell), osgEFT::Coord(0.0, cell), osgEFT::Coord(0.0, cell * 2), osgEFT::Coord(0.0, cell * 0));

        //line 2
        addDebugPass(m_em, "light_depth0"
            , osgEFT::Coord(0.0, cell), osgEFT::Coord(0.0, cell), osgEFT::Coord(0.0, cell * 0), osgEFT::Coord(0.0, cell * 1));
        addDebugPass(m_em, "light_depth1"
            , osgEFT::Coord(0.0, cell), osgEFT::Coord(0.0, cell), osgEFT::Coord(0.0, cell * 1), osgEFT::Coord(0.0, cell * 1));
        addDebugPass(m_em, "light_depth2"
            , osgEFT::Coord(0.0, cell), osgEFT::Coord(0.0, cell), osgEFT::Coord(0.0, cell * 2), osgEFT::Coord(0.0, cell * 1));
        addDebugPass(m_em, "light_depth3"
            , osgEFT::Coord(0.0, cell), osgEFT::Coord(0.0, cell), osgEFT::Coord(0.0, cell * 3), osgEFT::Coord(0.0, cell * 1));

    }

    m_enable = true;
}



int main(int argc, char** argv)
{
    //input file
    std::string input_filepath = "../data/";
    std::string filename = "house.obj";
    //std::string filename = "tire.obj";

    //create viewer
    osgViewer::Viewer viewer;
    viewer.setThreadingModel(osgViewer::Viewer::ThreadingModel::SingleThreaded);
    viewer.getCamera()->setClearColor(osg::Vec4(0.2, 0.2, 0.2, 1));
    viewer.getLight()->setPosition(osg::Vec4(1, 1, 1, 0));

    //set FOV
    //double fovy; double aspectRatio; double zNear; double zFar;
    //viewer.getCamera()->getProjectionMatrixAsPerspective(fovy, aspectRatio, zNear, zFar);
    //viewer.getCamera()->setProjectionMatrixAsPerspective(90.0, aspectRatio, zNear, zFar);

    /*
    * root
    *    scene_root->ss
    *       scene
    *    eft
    *       pass->ss
    */

    //root
    osg::ref_ptr<osg::Group> root = new osg::Group;
    viewer.setSceneData(root);

    //scene and scene_root
    osg::ref_ptr<osg::Group> scene_root = new osg::Group;
    osg::ref_ptr<osg::Group> scene = new osg::Group;
    root->addChild(scene_root);

    //model
    osg::Node* node = osgDB::readNodeFile(input_filepath + "/" + filename);
    {
        osgUtil::Optimizer op;
        op.optimize(node, 0xFFFFFFFF /*osgUtil::Optimizer::ALL_OPTIMIZATIONS*/);
    }
    scene->addChild(node);


    //
#if 1
    viewer.addEventHandler(new EffectScript(root, scene_root, scene, viewer.getLight(), viewer.getCamera()));
#else
    scene_root->addChild(scene);
#endif


    //
    //viewer.addEventHandler(new osgGA::StateSetManipulator(viewer.getCamera()->getOrCreateStateSet()));
    //viewer.addEventHandler(new osgViewer::ThreadingHandler);
    //viewer.addEventHandler(new osgViewer::WindowSizeHandler);
    //viewer.addEventHandler(new osgViewer::StatsHandler);
    //viewer.addEventHandler(new osgViewer::RecordCameraPathHandler);
    //viewer.addEventHandler(new osgViewer::LODScaleHandler);
    //viewer.addEventHandler(new osgViewer::ScreenCaptureHandler);


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

