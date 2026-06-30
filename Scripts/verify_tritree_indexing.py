"""
验证 FSphereTopology::PrimalTriTreeNodes 数组的索引代数公式

参考 FSphereTopology.cpp:
  - BuildIcosahedronUnit: 创建 20 个根节点，存入 TriTreeRoots[] 和 PrimalTriTreeNodes[]
  - SubdividePrimalOnce: 遍历当前 PrimalTriTreeNodes，对每个 Index 创建 4 子，
    NewPrimalTriTreeNodes 按 (Index*4+0..3) 顺序追加，最后整体替换 PrimalTriTreeNodes

最终态（sub=N 调用 SubdividePrimalOnce N 次后）：
  - PrimalTriTreeNodes 只保留第 N 层的所有叶子，共 20 * 4^N 个
  - 根（20 个）只在 TriTreeRoots[]，不在 PrimalTriTreeNodes
  - 中间层节点游离在 Father 链中，不在任何数组里

本脚本同时验证两个视角：
  视角 A（实际实现）：'PrimalTriTreeNodes 只存最深叶子层' 视角
                      → 用'森林根索引 + 子树局部 4 叉堆下标'公式
  视角 B（用户洞察）：'全局节点编号、按层广度优先平铺、相邻 4 子 + 跳过上层'视角
                      → 用'起始偏移 + 4*Index + k - 偏移'公式

两个视角等价，但 B 更直观地呈现了'森林同层连续'的几何性质。
"""


def simulate_build(N: int):
    """模拟 BuildIcosahedronUnit + SubdividePrimalOnce N 次后的 PrimalTriTreeNodes"""
    nodes = []
    for r in range(20):
        nodes.append({
            'global_id': len(nodes),
            'father_id': None,
            'depth': 0,
            'root_id': r,
            'local_idx_in_root_subtree': 0,
            'depth_layer_idx': r,  # 视角 B：在第 0 层的全局位置
        })
    tri_tree_roots = [n['global_id'] for n in nodes]
    primal = list(tri_tree_roots)

    for sub_step in range(N):
        new_primal = []
        for index, parent_id in enumerate(primal):
            for k in range(4):
                child = {
                    'global_id': len(nodes),
                    'father_id': parent_id,
                    'depth': nodes[parent_id]['depth'] + 1,
                    'root_id': nodes[parent_id]['root_id'],
                    'local_idx_in_root_subtree':
                        nodes[parent_id]['local_idx_in_root_subtree'] * 4 + k,
                    # 视角 B：第 (sub_step+1) 层的全局位置
                    # 父在第 sub_step 层、深度全局位置 = nodes[parent_id]['depth_layer_idx']
                    # 子在第 (sub_step+1) 层、深度全局位置 = parent_layer_idx * 4 + k
                    'depth_layer_idx':
                        nodes[parent_id]['depth_layer_idx'] * 4 + k,
                }
                nodes.append(child)
                new_primal.append(child['global_id'])
        primal = new_primal

    return primal, tri_tree_roots, nodes


# ============================================================================
# 视角 A：'PrimalTriTreeNodes 只存最深叶子层'公式
# ============================================================================

def perspective_A(N: int):
    """
    视角 A：实际数据结构。

    PrimalTriTreeNodes 只包含第 N 层叶子（共 20*4^N 个），按 root 0..19 串联。
    给定数组下标 idx (0 <= idx < 20*4^N)：
      RootId       = idx // 4^N            (所属根 Tri，对应 TriTreeRoots[RootId])
      LocalLeafIdx = idx % 4^N             (在该 root 子树中是第几个叶子)

    爬升 K 步（不跨数组、只走 Father 指针）：
      K == 0           : 自身（PrimalTriTreeNodes[idx]）
      K == N           : 根节点（TriTreeRoots[RootId]，**不在** PrimalTriTreeNodes）
      0 < K < N        : 中间层节点，**不在任何数组**——只能通过 Father 链访问
                         其在 root 子树第 (N-K) 层的局部下标 = LocalLeafIdx // 4^K

    同层兄弟（4 个共父）：
      Sibling 起点 = (idx // 4) * 4
      LocalK (该叶子是父第几个 child) = idx % 4
    """
    primal, roots, nodes = simulate_build(N)
    leaves_per_root = 4 ** N

    for idx, node_id in enumerate(primal):
        node = nodes[node_id]
        assert node['root_id'] == idx // leaves_per_root
        assert node['local_idx_in_root_subtree'] == idx % leaves_per_root

    # 抽样验证爬升公式
    for idx in range(0, len(primal), max(1, len(primal) // 100)):
        node = nodes[primal[idx]]
        cur = node['global_id']
        for K in range(1, N + 1):
            cur = nodes[cur]['father_id']
            if K == N:
                assert cur == roots[node['root_id']]
            else:
                expected_local = node['local_idx_in_root_subtree'] // (4 ** K)
                assert nodes[cur]['local_idx_in_root_subtree'] == expected_local
                assert nodes[cur]['root_id'] == node['root_id']

    print(f'  [视角 A] sub={N}: PASS')


# ============================================================================
# 视角 B：'全局编号 + 按层广度优先平铺 + 跳过上层'公式
# ============================================================================

def perspective_B(N: int):
    """
    视角 B：用户提出的'森林全局编号'视角。

    把所有层节点平铺到一个全局编号空间，按 (深度, 同层位置) 字典序排列：
      第 0 层（root 层）  : 全局编号 [0 .. 19]            （20 个根）
      第 1 层（首次细分） : 全局编号 [20 .. 99]            （80 = 20*4 个）
      第 d 层             : 全局编号 [Offset(d) .. Offset(d+1)-1]
                            其中 Offset(d) = sum(20 * 4^i for i in 0..d-1)
                                            = 20 * (4^d - 1) / 3

    给定第 d 层节点的全局编号 G：
      LayerLocalIdx = G - Offset(d)        (该节点是第 d 层的第几个，从 0 起编)
      子节点（在第 d+1 层）全局编号:
          ChildG[k] = Offset(d+1) + LayerLocalIdx * 4 + k     (k = 0..3)
      父节点（在第 d-1 层）全局编号:
          FatherG = Offset(d-1) + LayerLocalIdx // 4

    用户给的例子需要修正为：
      "0 号节点（root 0）的 4 个子节点不是 1,2,3,4，而是
       Offset(1) + 0*4 + 0..3 = 20+0..3 = 20, 21, 22, 23"
       (用户说的是 12,13,14,15——他记成了 12 个根；实际是 20 个根，所以是 20,21,22,23)

    该公式与视角 A 等价 —— 如果只看第 N 层的所有节点，把它们的全局编号减去 Offset(N)，
    就得到 '在 PrimalTriTreeNodes 中的下标'。
    """
    primal, roots, nodes = simulate_build(N)

    # 计算每层 offset
    def offset(d: int) -> int:
        return sum(20 * (4 ** i) for i in range(d))

    # 给所有节点（不只是叶子）按层 BFS 编号
    layer_offset_check = [offset(d) for d in range(N + 2)]
    # 验证：第 d 层的所有节点的'按层 BFS 全局编号'应该是连续的
    # 且 'depth_layer_idx' 已经在 simulate_build 中维护，等于'该节点是第 d 层的第几个'
    for d in range(N + 1):
        offset_d = offset(d)
        nodes_at_d = [n for n in nodes if n['depth'] == d]
        # 验证个数 = 20 * 4^d
        assert len(nodes_at_d) == 20 * (4 ** d), \
            f'第 {d} 层节点数 = {len(nodes_at_d)}, 期望 {20*(4**d)}'
        # 验证 depth_layer_idx 唯一且覆盖 [0, len)
        layer_idxs = sorted(n['depth_layer_idx'] for n in nodes_at_d)
        assert layer_idxs == list(range(20 * (4 ** d))), \
            f'第 {d} 层 depth_layer_idx 不是 [0, {20*(4**d)})'

    # 验证'子节点全局编号 = Offset(d+1) + 父layer_local * 4 + k'
    for n in nodes:
        if n['depth'] == 0:
            continue
        father = nodes[n['father_id']]
        d_father = father['depth']
        d_self = n['depth']
        father_layer_local = father['depth_layer_idx']
        # 子节点是父的第几个 child？由 local_idx_in_root_subtree 末两位决定
        k = n['local_idx_in_root_subtree'] % 4
        # 子节点的 layer_local_idx 应等于 father_layer_local * 4 + k
        expected = father_layer_local * 4 + k
        assert n['depth_layer_idx'] == expected, \
            f'depth={d_self} k={k}: depth_layer_idx={n["depth_layer_idx"]}, expected {expected}'

    # 验证'最深层叶子的全局编号 - Offset(N) = PrimalTriTreeNodes 下标'
    for idx, leaf_id in enumerate(primal):
        leaf = nodes[leaf_id]
        global_id_in_layer_bfs = offset(N) + leaf['depth_layer_idx']
        # PrimalTriTreeNodes 下标 = 全局编号 - Offset(N) = depth_layer_idx
        assert leaf['depth_layer_idx'] == idx, \
            f'idx={idx}: depth_layer_idx={leaf["depth_layer_idx"]}'

    print(f'  [视角 B] sub={N}: PASS  (Offset 公式 = sum(20*4^i for i in 0..d-1) = 20*(4^d-1)/3)')


def cross_root_climb_test(N: int):
    """跨 root 不能爬到公共父节点"""
    primal, roots, nodes = simulate_build(N)
    leaves_per_root = 4 ** N
    if N == 0:
        return  # sub=0 时叶子就是根，无可爬
    idx_a, idx_b = 0, leaves_per_root
    cur_a, cur_b = primal[idx_a], primal[idx_b]
    for K in range(1, N + 1):
        cur_a = nodes[cur_a]['father_id']
        cur_b = nodes[cur_b]['father_id']
    assert cur_a == roots[0]
    assert cur_b == roots[1]
    assert nodes[cur_a]['father_id'] is None
    assert nodes[cur_b]['father_id'] is None
    print(f'  [跨 root]  sub={N}: PASS')


def show_examples():
    """打印 sub=3 的几个示例，方便对照设计稿"""
    print('\n========== sub=3 示例（用于设计稿展示） ==========')
    primal, roots, nodes = simulate_build(3)
    print(f'PrimalTriTreeNodes 长度 = {len(primal)} = 20 * 4^3')
    print(f'TriTreeRoots 长度 = {len(roots)} = 20')
    print()

    print('视角 A：PrimalTriTreeNodes (sub=3) 数组分段')
    for r in [0, 1, 2, 19]:
        start = r * 64
        end = (r + 1) * 64 - 1
        print(f'  [{start:4d} .. {end:4d}] = root {r} 子树的 64 个叶子')

    print('\n视角 B：森林全局 BFS 编号偏移')
    for d in range(4):
        offset_d = sum(20 * (4 ** i) for i in range(d))
        count = 20 * (4 ** d)
        print(f'  第 {d} 层（depth={d}）: 偏移 {offset_d:5d}, 节点数 {count:5d} '
              f'(全局编号 [{offset_d}..{offset_d+count-1}])')

    print('\n视角 B 用户原始例子的修正：')
    print('  "0 号节点（root 0）的 4 个子节点全局编号"')
    print('   = Offset(1) + 0*4 + 0..3 = 20+0..3 = 20, 21, 22, 23')
    print('  （用户记的 12,13,14,15 把根数说成了 12——正二十面体实际是 20 个根面）')

    print('\n视角 A 爬升示例：idx=130 → 爬到 TriTreeRoots[2]')
    idx = 130
    cur = primal[idx]
    print(f'  idx={idx}, RootId={idx//64}, LocalLeafIdx={idx%64}')
    for K in range(1, 4):
        cur = nodes[cur]['father_id']
        if K < 3:
            local = (idx % 64) // (4 ** K)
            print(f'    K={K}: 中间节点（不在数组里），root 2 子树第 {3-K} 层局部下标={local}')
        else:
            print(f'    K=3: 到达 TriTreeRoots[{idx//64}]')


if __name__ == '__main__':
    print('Verifying PrimalTriTreeNodes 索引代数公式 ...')
    for N in [0, 1, 2, 3, 4, 5]:
        perspective_A(N)
        perspective_B(N)
        cross_root_climb_test(N)
    show_examples()
    print('\nALL TESTS PASSED.')
