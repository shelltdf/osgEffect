
/*

*/


#include <osgDB/ReadFile>
#include <osgViewer/Viewer>
#include <osgGA/StateSetManipulator>
#include <osgGA/TrackballManipulator>
#include <osgViewer/ViewerEventHandlers>
#include <osg/MatrixTransform>
#include <osgUtil/Optimizer>
#include <osgUtil/TangentSpaceGenerator>
#include <osg/ShapeDrawable>

#include <osgEFT/EffectManager>
#include <osgEFT/NFX>


class TangentVisitor
    : public osg::NodeVisitor
{
public:

    TangentVisitor() :
        osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
    {}

    virtual ~TangentVisitor(){}

    virtual void apply(osg::Geometry& geometry)
    {
        prepareGeometry(&geometry);
        traverse(geometry);
    }

    void prepareGeometry(osg::Geometry* geo)
    {
        osg::ref_ptr<osgUtil::TangentSpaceGenerator> tsg = 
            new osgUtil::TangentSpaceGenerator;
        tsg->generate(geo, 0);

        if (!geo->getVertexAttribArray(6))
            geo->setVertexAttribArray(6, tsg->getTangentArray());
    }
};



void applySufaceMaterial(osg::StateSet* ss, const std::string& filename)
{
    osg::ref_ptr<osgEFT::NFX> nfx = new osgEFT::NFX();

    if (nfx->load(filename))
    {
        osg::StateSet* sm = nfx->getSufaceMaterial();
        if (sm)
        {
            ss->merge(*sm);
        }
    }
}

int main(int argc, char** argv)
{
    //input file
    std::string model_filename = "../data/tire.obj";
    std::string nfx_filename = "../data/nfx_text.xml";

    if (argc >= 2)
    {
        nfx_filename = argv[1];
    }

    //create viewer
    osgViewer::Viewer viewer;
    viewer.setThreadingModel(osgViewer::Viewer::ThreadingModel::SingleThreaded);
    viewer.getCamera()->setClearColor(osg::Vec4(0.2, 0.2, 0.2, 1));
    viewer.getLight()->setPosition(osg::Vec4(1, 1, 1, 0));

    //root
    osg::ref_ptr<osg::Group> root = new osg::Group;
    viewer.setSceneData(root);

    //model
    osg::Node* node = osgDB::readNodeFile(model_filename);
    if (node == 0)
    {
        //return 0;

        osg::ShapeDrawable* sd = 
            new osg::ShapeDrawable(new osg::Sphere(osg::Vec3(0, 0, 0), 1.0));
        osg::Geode* geode = new osg::Geode();
        geode->addDrawable(sd);

        node = geode;
    }
    else
    {
        osgUtil::Optimizer op;
        op.optimize(node, 0xFFFFFFFF /*osgUtil::Optimizer::ALL_OPTIMIZATIONS*/);
    }
    root->addChild(node);

    //
    TangentVisitor tv;
    root->accept(tv);

    //
    applySufaceMaterial(node->getOrCreateStateSet(), nfx_filename);


    //
    viewer.addEventHandler(new osgGA::StateSetManipulator(viewer.getCamera()->getOrCreateStateSet()));
    viewer.addEventHandler(new osgViewer::ThreadingHandler);
    viewer.addEventHandler(new osgViewer::WindowSizeHandler);
    viewer.addEventHandler(new osgViewer::StatsHandler);
    viewer.addEventHandler(new osgViewer::RecordCameraPathHandler);
    viewer.addEventHandler(new osgViewer::LODScaleHandler);
    viewer.addEventHandler(new osgViewer::ScreenCaptureHandler);


    //realize
    viewer.setUpViewInWindow(100, 100, 1280, 720);
    viewer.run();

    return 0;
}

