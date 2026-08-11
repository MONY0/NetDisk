USE NetDisk;

-- 重复手机号：有结果时不要直接创建 uk_user_tel。
SELECT u_tel, COUNT(*) AS duplicate_count,
       GROUP_CONCAT(u_id ORDER BY u_id) AS user_ids
  FROM t_user
 GROUP BY u_tel
HAVING COUNT(*) > 1;

-- 同一用户、同一路径下的重名记录：有结果时不要直接创建唯一索引。
SELECT u_id, f_dir, f_name, COUNT(*) AS duplicate_count,
       GROUP_CONCAT(f_id ORDER BY f_id) AS file_ids
  FROM t_user_file
 GROUP BY u_id, f_dir, f_name
HAVING COUNT(*) > 1;

-- 孤立的用户文件关系。
SELECT uf.*
  FROM t_user_file uf
  LEFT JOIN t_file f ON f.f_id = uf.f_id
 WHERE f.f_id IS NULL;

-- 数据库有记录，但服务器物理文件是否存在，需要在 Linux 文件系统继续检查。
SELECT f_id, f_path, f_count, f_state, f_type
  FROM t_file
 WHERE f_count <= 0;

