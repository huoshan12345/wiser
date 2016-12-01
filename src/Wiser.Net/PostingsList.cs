using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace Wiser.Net
{
    public class PostingsList
    {
        public int DocumentId { get; set; }             /* 文档编号 */
        public List<int> Positions { get; set; }        /* 位置信息的数组 */
        public int PositionsCount { get; set; }        /* 位置信息的条数 */
        public PostingsList Next { get; set; }/* 指向下一个倒排列表的指针 */
    }
}
