
#include<osgEFT/Pass>
#include<osgEFT/PassUpdater>

using namespace osgEFT;


void Pass::setPassUpdater(PassUpdater* pu)
{
    //camera->setEventCallback(pu);
    //camera->setUpdateCallback(pu);
    //camera->addCullCallback(pu);


    //camera->setInitialDrawCallback(pu);

    //camera->setFinalDrawCallback(pu);

    pu->pass = this;
    pu->onSetup(camera);
}

