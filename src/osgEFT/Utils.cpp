
#include<osgEFT/Utils>

using namespace osgEFT;


//ͳ��geode����
class getGeodeNum
    :public osg::NodeVisitor
{
public:
    getGeodeNum()
        :osg::NodeVisitor(osg::NodeVisitor::TraversalMode::TRAVERSE_ALL_CHILDREN)
        ,num(0)
    {}
    virtual ~getGeodeNum() {}

    virtual void apply(osg::Geode& geode)
    {
        num++;
        traverse(geode);
    }
//private:
    int num;
};

//��¼geode��matrix
class SelectGeodeAndMatrix
    :public osg::NodeVisitor
{
public:
    SelectGeodeAndMatrix( int num )
        :osg::NodeVisitor(osg::NodeVisitor::TraversalMode::TRAVERSE_ALL_CHILDREN)
        //, m_filepath(filepath)
    {
        m_num_index = 0;
        matrix_list.resize(num);
        geode_list.resize(num);
    }
    virtual ~SelectGeodeAndMatrix() {}

    virtual void apply(osg::Transform& transform)
    {
        osg::Matrix m = m_matrix;
        transform.computeLocalToWorldMatrix(m_matrix, 0);
        traverse(transform);
        m_matrix = m;
    }

    virtual void apply(osg::Geode& geode)
    {
        matrix_list[m_num_index] = m_matrix;
        geode_list[m_num_index] = &geode;
        m_num_index++;
        traverse(geode);
    }

    //osg::BoundingBox getBBox() { return m_bbox; }
    std::vector< osg::Matrix > matrix_list;
    std::vector< osg::ref_ptr<osg::Geode> > geode_list;

private:
    //std::string m_filepath;//�����ļ�Ŀ¼
    osg::Matrix m_matrix;//��ǰ����
    int m_num_index;
    //osg::BoundingBox m_bbox;//ȫ��ģ�͵�bbox
};

//����һ��geode�Ŀռ���Ϣ
struct GEODE_INFO
{
    osg::ref_ptr<osg::Geode> geode;
    osg::Matrix matrix;
};



//�ݹ�˲���
//  ����ֵ �Ƿ�ɹ�����plod�ļ�
void recursiveOctree(
    osg::Group* parent
    , const std::vector<GEODE_INFO>& info       //���ݱ�
    , const osg::BoundingBox bbox               //��ǰ����bbox����
    , int level = 0                             //����
    , QUADRANT_TYPE quadrant = OT_Null          //��ǰ����
    , const std::string str = "p"               //�����ַ���
)
{
    std::ostringstream oss;
    //oss << "L" << level << "Q" << quadrant << " " << bbox.radius();
    oss << "L" << level << "Q" << quadrant << " " << bbox.xMax()-bbox.xMin();
    oss << " -- " << str.c_str();

    std::string filename = oss.str();
    std::cout << filename << std::endl;

    //������ ����ֹ�ݹ�
    if (info.size() == 0) return;// GEODE_INFO();
    int info_size = info.size();

    //�����м�ֵ
    double median_x = (bbox.xMax() - bbox.xMin()) / 2.0 + bbox.xMin();
    double median_y = (bbox.yMax() - bbox.yMin()) / 2.0 + bbox.yMin();
    double median_z = (bbox.zMax() - bbox.zMin()) / 2.0 + bbox.zMin();

    //�ӷ����ɹ����
    //int save_file_count = 0;
    std::map<std::string/*filename*/, GEODE_INFO> this_info; //��¼��ǰ�ڵ���ӽڵ���Ϣ

    //ѭ��8������
    for (int i = OT_XYZ; i <= OT_XnYnZ; i++)
    {
        //���㵱ǰ���޵ı߽�
        osg::BoundingBox bbox_current;
        if (i == OT_XYZ)//1
        {
            bbox_current.set(osg::Vec3(median_x, median_y, median_z), osg::Vec3(bbox.xMax(), bbox.yMax(), bbox.zMax()));
        }
        else if (i == OT_nXYZ)//2
        {
            bbox_current.set(osg::Vec3(bbox.xMin(), median_y, median_z), osg::Vec3(median_x, bbox.yMax(), bbox.zMax()));
        }
        else if (i == OT_nXnYZ)//3
        {
            bbox_current.set(osg::Vec3(bbox.xMin(), bbox.yMin(), median_z), osg::Vec3(median_x, median_y, bbox.zMax()));
        }
        else if (i == OT_XnYZ)//4
        {
            bbox_current.set(osg::Vec3(median_x, bbox.yMin(), median_z), osg::Vec3(bbox.xMax(), median_y, bbox.zMax()));
        }
        else if (i == OT_XYnZ)//5
        {
            bbox_current.set(osg::Vec3(median_x, median_y, bbox.zMin()), osg::Vec3(bbox.xMax(), bbox.yMax(), median_z));
        }
        else if (i == OT_nXYnZ)//6
        {
            bbox_current.set(osg::Vec3(bbox.xMin(), median_y, bbox.zMin()), osg::Vec3(median_x, bbox.yMax(), median_z));
        }
        else if (i == OT_nXnYnZ)//7
        {
            bbox_current.set(osg::Vec3(bbox.xMin(), bbox.yMin(), bbox.zMin()), osg::Vec3(median_x, median_y, median_z));
        }
        else if (i == OT_XnYnZ)//8
        {
            bbox_current.set(osg::Vec3(median_x, bbox.yMin(), bbox.zMin()), osg::Vec3(bbox.xMax(), median_y, median_z));
        }


        //test
        //if (!bbox_current.valid())
        //{
        //    printf("v");
        //}

        //���㵱ǰ���ް����Ķ���
        std::vector< GEODE_INFO> child_info;
        //auto b = info.begin();
        size_t e = info.size();
        for (size_t i = 0; i < e; i++)
        {
            osg::Vec3 wc = info[i].geode->getBound().center() * info[i].matrix;
            //if (bbox_current.contains(wc))
            if(contains(bbox_current, wc))
            {
                child_info.push_back(info[i]);
            }
        }
        info_size -= child_info.size();


        //�ӽڵ�����
        std::ostringstream oss;
        oss << str.c_str() << i;
        std::string s2 = oss.str();


        //���ϸ�������С ���������ڵĶ������
        float r = bbox_current.radius()*2.0;
        if (
            bbox_current.radius()*2.0 < 10.0
            || child_info.size() <= 8
            )
        {
            //����Ҷ�ļ�

            //�����ļ���
            std::ostringstream oss;
            //oss << "L" << level << "_Q" << i << " " << bbox_current.radius();
            //oss << str << "[" << info_current.size() << "]";
            oss << "leaf_" <<str << QUADRANT_TYPE(i);
            std::string filename = oss.str();

            //�ж��������Ч����
            if (child_info.size() > 0) 
            {
                //����Ҷ�ӽڵ�
                osg::Group* this_group = new osg::Group();
                parent->addChild(this_group);
                this_group->setName(filename);

                e = child_info.size();
                for (size_t i = 0; i < e; i++)
                {
                    if(child_info[i].matrix == osg::Matrix::identity())
                    { 
                        this_group->addChild(child_info[i].geode);
                    }
                    else
                    {
                        osg::MatrixTransform* mt = new osg::MatrixTransform(child_info[i].matrix);
                        mt->addChild(child_info[i].geode);
                        this_group->addChild(mt);
                    }
                }
            }
        }
        else//�����ǰ������Ȼ����ϸ��
        {
            //�ݹ�������
            osg::Group* this_group = new osg::Group();
            parent->addChild(this_group);
            /*GEODE_INFO s = */recursiveOctree(this_group, child_info, bbox_current, level + 1, QUADRANT_TYPE(i), s2);
        }
    }

    //����ڵ��ļ�
    //BIM_MODEL_INFO ret;
    //if (this_info.size() > 0)
    //{
    //    std::cout << "[" << str.c_str() << "]" << std::endl;
    //    ret = createAndSavePLOD(str, this_info, false);
    //}

    //ret.filename = str;
    //ret.bbox = 

    if (info_size > 0)
    {
        std::cout << "error info_size :" << info_size << std::endl;
    }


    //����κ����ļ�������ɹ�
    //return this_info;// save_file_count > 0;
    //return ret;
}


osg::Group* osgEFT::newOctree(osg::Group* in_group)
{
    osg::Group* ret = new osg::Group();
    ret->setName("OctreeRoot");

    //����ȫ��geode����;����
    osg::ref_ptr<getGeodeNum> getgeodenum = new getGeodeNum();
    in_group->accept(*getgeodenum);
    osg::ref_ptr<SelectGeodeAndMatrix> selectgeodeandmatrix = new SelectGeodeAndMatrix(getgeodenum->num);
    in_group->accept(*selectgeodeandmatrix);


    //ת��������ʽ
    std::vector<GEODE_INFO> info;
    info.resize(selectgeodeandmatrix->geode_list.size());
    for (size_t i = 0; i < selectgeodeandmatrix->geode_list.size(); i++)
    {
        GEODE_INFO this_info;
        this_info.geode = selectgeodeandmatrix->geode_list[i];
        this_info.matrix = selectgeodeandmatrix->matrix_list[i];
        info[i] = this_info;
    }


    //���հ˲�������µĳ���ͼ
    double length = 1024*100;
    recursiveOctree(ret, info
        , osg::BoundingBox(-length / 2.0, -length / 2.0, -length / 2.0, length / 2.0, length / 2.0, length / 2.0)
    );

    return ret;
}



