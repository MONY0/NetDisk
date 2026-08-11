USE NetDisk;

-- 1. 修复删除触发器：这里只减少引用计数。
-- 物理文件和 t_file 元数据由服务端在确认删除成功后处理。
DROP TRIGGER IF EXISTS refMinusOne;
DELIMITER //
CREATE TRIGGER refMinusOne
AFTER DELETE ON t_user_file
FOR EACH ROW
BEGIN
    UPDATE t_file
       SET f_count = GREATEST(f_count - 1, 0)
     WHERE f_id = OLD.f_id;
END //
DELIMITER ;

-- 2. 重建新增关系触发器，确保定义统一。
DROP TRIGGER IF EXISTS refPlusOne;
DELIMITER //
CREATE TRIGGER refPlusOne
AFTER INSERT ON t_user_file
FOR EACH ROW
BEGIN
    UPDATE t_file
       SET f_count = f_count + 1
     WHERE f_id = NEW.f_id;
END //
DELIMITER ;

-- 3. 常用查询索引。每条只执行一次；若索引已经存在，MySQL 会提示重名。
ALTER TABLE t_file
    ADD INDEX idx_file_md5_state (f_MD5, f_state);

ALTER TABLE t_user_file
    ADD INDEX idx_user_dir (u_id, f_dir),
    ADD INDEX idx_file_id (f_id),
    ADD INDEX idx_share_link (s_link);

-- 4. 按真实关系重新计算引用计数，修复历史不一致。
UPDATE t_file f
LEFT JOIN (
    SELECT f_id, COUNT(*) AS real_count
      FROM t_user_file
     GROUP BY f_id
) uf ON uf.f_id = f.f_id
SET f.f_count = COALESCE(uf.real_count, 0);

-- 5. 验证结果。
SHOW TRIGGERS FROM NetDisk;

SELECT f.f_id, f.f_count, COUNT(uf.f_id) AS real_count
  FROM t_file f
  LEFT JOIN t_user_file uf ON uf.f_id = f.f_id
 GROUP BY f.f_id, f.f_count
HAVING f.f_count <> real_count;

-- 以下唯一约束必须先执行重复数据检查，确认结果为空后再手动执行。
-- ALTER TABLE t_user
--     ADD UNIQUE INDEX uk_user_tel (u_tel);
--
-- ALTER TABLE t_user_file
--     ADD UNIQUE INDEX uk_user_file_name (u_id, f_dir, f_name);

