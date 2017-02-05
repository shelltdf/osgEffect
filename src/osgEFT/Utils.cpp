
#include<osgEFT/Utils>

using namespace osgEFT;


//统计geode数量
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

//记录geode和matrix
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
    //std::string m_filepath;//输入文件目录
    osg::Matrix m_matrix;//当前矩阵
    int m_num_index;
    //osg::BoundingBox m_bbox;//全部模型的bbox
};

//基于一个geode的空间信息
struct GEODE_INFO
{
    osg::ref_ptr<osg::Geode> geode;
    osg::Matrix matrix;
};



//递归八叉树
//  返回值 是否成功保存plod文件
void recursiveOctree(
    osg::Group* parent
    , const std::vector<GEODE_INFO>& info       //数据表
    , const osg::BoundingBox bbox               //当前数据bbox区域
    , int level = 0                             //级别
    , QUADRANT_TYPE quadrant = OT_Null          //当前区域
    , const std::string str = "p"               //名称字符串
)
{
    std::ostringstream oss;
    //oss << "L" << level << "Q" << quadrant << " " << bbox.radius();
    oss << "L" << level << "Q" << quadrant << " " << bbox.xMax()-bbox.xMin();
    oss << " -- " << str.c_str();

    std::string filename = oss.str();
    std::cout << filename << std::endl;

    //无数据 就终止递归
    if (info.size() == 0) return;// GEODE_INFO();
    int info_size = info.size();

    //计算中间值
    double median_x = (bbox.xMax() - bbox.xMin()) / 2.0 + bbox.xMin();
    double median_y = (bbox.yMax() - bbox.yMin()) / 2.0 + bbox.yMin();
    double median_z = (bbox.zMax() - bbox.zMin()) / 2.0 + bbox.zMin();

    //子分区成功标记
    //int save_file_count = 0;
    std::map<std::string/*filename*/, GEODE_INFO> this_info; //记录当前节点的子节点信息

    //循环8个象限
    for (int i = OT_XYZ; i <= OT_XnYnZ; i++)
    {
        //计算当前象限的边界
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

        //计算当前象限包含的对象
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


        //子节点名称
        std::ostringstream oss;
        oss << str.c_str() << i;
        std::string s2 = oss.str();


        //如果细分区域过小 或者区域内的对象过少
        float r = bbox_current.radius()*2.0;
        if (
            bbox_current.radius()*2.0 < 10.0
            || child_info.size() <= 8
            )
        {
            //保存叶文件

            //生成文件名
            std::ostringstream oss;
            //oss << "L" << level << "_Q" << i << " " << bbox_current.radius();
            //oss << str << "[" << info_current.size() << "]";
            oss << "leaf_" <<str << QUADRANT_TYPE(i);
            std::string filename = oss.str();

            //有对象才能有效保存
            if (child_info.size() > 0) 
            {
                //生成叶子节点
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
        else//如果当前区域仍然可以细分
        {
            //递归子区域
            osg::Group* this_group = new osg::Group();
            parent->addChild(this_group);
            /*GEODE_INFO s = */recursiveOctree(this_group, child_info, bbox_current, level + 1, QUADRANT_TYPE(i), s2);
        }
    }

    //保存节点文件
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


    //如果任何子文件被保存成功
    //return this_info;// save_file_count > 0;
    //return ret;
}


osg::Group* osgEFT::newOctree(osg::Group* in_group)
{
    osg::Group* ret = new osg::Group();
    ret->setName("OctreeRoot");

    //建立全部geode对象和矩阵表
    osg::ref_ptr<getGeodeNum> getgeodenum = new getGeodeNum();
    in_group->accept(*getgeodenum);
    osg::ref_ptr<SelectGeodeAndMatrix> selectgeodeandmatrix = new SelectGeodeAndMatrix(getgeodenum->num);
    in_group->accept(*selectgeodeandmatrix);


    //转换数据形式
    std::vector<GEODE_INFO> info;
    info.resize(selectgeodeandmatrix->geode_list.size());
    for (size_t i = 0; i < selectgeodeandmatrix->geode_list.size(); i++)
    {
        GEODE_INFO this_info;
        this_info.geode = selectgeodeandmatrix->geode_list[i];
        this_info.matrix = selectgeodeandmatrix->matrix_list[i];
        info[i] = this_info;
    }


    //按照八叉树组件新的场景图
    double length = 1024*100;
    recursiveOctree(ret, info
        , osg::BoundingBox(-length / 2.0, -length / 2.0, -length / 2.0, length / 2.0, length / 2.0, length / 2.0)
    );

    return ret;
}



