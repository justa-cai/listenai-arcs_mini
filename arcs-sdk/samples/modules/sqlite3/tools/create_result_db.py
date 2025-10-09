import sqlite3
import os
import random
import time
import argparse
import subprocess
import shutil

# 数据库文件名
db_filename = 'test.db'
db_encryption_filename = 'test_csk_enc.db'

def create_database():
    # 如果数据库文件已存在，先删除它
    if os.path.exists(db_filename):
        os.remove(db_filename)
        print(f"Existing {db_filename} removed.")

    # 创建数据库连接
    conn = sqlite3.connect(db_filename)
    cursor = conn.cursor()

    cursor.execute("""
    PRAGMA journal_mode = DELETE;
    """)

    # 创建 p_characters 表
    cursor.execute("""
    CREATE TABLE IF NOT EXISTS p_characters (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        age INTEGER NOT NULL,
        score REAL NOT NULL
    );
    """)

    # 提交更改并关闭连接
    conn.commit()
    conn.close()

    print(f"Database '{db_filename}' created successfully with 'p_characters' table.")

def generate_random_data(num_records=50000):
    # 生成随机姓氏
    surnames = ['张', '王', '李', '赵', '刘', '陈', '杨', '黄', '周', '吴']
    # 生成随机名字
    names = ['伟', '芳', '娜', '秀英', '敏', '静', '丽', '强', '磊', '军']
    
    # 连接到数据库
    conn = sqlite3.connect(db_filename)
    cursor = conn.cursor()

    start_time = time.time()
    
    # 使用事务来提高插入速度
    cursor.execute("BEGIN TRANSACTION")
    
    try:
        # 批量插入数据
        batch_size = 1000
        for i in range(0, num_records, batch_size):
            batch_data = [
                (
                    random.choice(surnames) + random.choice(names),  # 随机姓名
                    random.randint(18, 50),                         # 随机年龄 18-50
                    round(random.uniform(60.0, 100.0), 1)          # 随机分数 60.0-100.0
                )
                for _ in range(min(batch_size, num_records - i))
            ]
            
            cursor.executemany(
                "INSERT INTO p_characters (name, age, score) VALUES (?, ?, ?)",
                batch_data
            )
            
            if (i + batch_size) % 10000 == 0:
                print(f"Inserted {i + batch_size} records...")
        
        # 提交事务
        conn.commit()
        
        end_time = time.time()
        print(f"\nSuccessfully inserted {num_records} records in {round(end_time - start_time, 2)} seconds.")
        
        # 验证记录数
        cursor.execute("SELECT COUNT(*) FROM p_characters")
        count = cursor.fetchone()[0]
        print(f"Total records in database: {count}")
        
    except Exception as e:
        conn.rollback()
        print(f"Error occurred: {e}")
    finally:
        conn.close()

class DatabaseEncryptor:
    def __init__(self, encryption_key):
        self.encryption_key = encryption_key

    def encrypt_database(self, input_db_path, output_db_path):
        if not os.path.exists(input_db_path):
            print(f"未找到输入数据库：{input_db_path}")
            return

        if os.path.exists(output_db_path):
            os.remove(output_db_path)

        try:
            shutil.copy2(input_db_path, output_db_path)
            print(f"文件已成功复制到 {output_db_path}")
        except FileNotFoundError:
            print(f"找不到文件：{input_db_path}")
        except PermissionError:
            print(f"没有权限访问文件：{output_db_path}")
        except Exception as e:
            print(f"复制过程中出错：{e}")

        try:
            # 构建 sqleet 命令
            command = [
                './sqleet', 
                output_db_path, 
                f'PRAGMA rekey = "{self.encryption_key}";'
            ]

            # 执行命令
            subprocess.run(command, check=True, shell=False)

            print(f"数据库{output_db_path}已重新加密并更新密码：{self.encryption_key}")

        except subprocess.CalledProcessError as e:
            print(f"处理过程中出错：{e}")


def main():
    parser = argparse.ArgumentParser(description="Encrypt an SQLite database using SQLCipher.")
    parser.add_argument("-key", nargs='?', default="123456", help="Encryption key for the database.")
    args = parser.parse_args()

    create_database()
    generate_random_data()
    encryptor = DatabaseEncryptor(args.key)
    encryptor.encrypt_database(db_filename, db_encryption_filename)

if __name__ == '__main__':
    main()
