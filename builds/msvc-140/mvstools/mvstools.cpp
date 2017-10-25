// mvstools.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>

#include <boost/filesystem.hpp>

#include <metaverse/bitcoin/utility/path.hpp>
#include <metaverse/bitcoin/wallet/settings.hpp>
#include <metaverse/database/databases/account_database.hpp>

using namespace libbitcoin;

inline hash_digest get_hash(const std::string str)
{
    data_chunk data(str.begin(), str.end());
    return sha256_hash(data);
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " username password" << std::endl;
        return -1;
    }
    hash_digest name_hash = get_hash(argv[1]);
    hash_digest passwd_hash = get_hash(argv[2]);

    auto home_path = default_data_path();
    auto data_path = home_path / "mainnet/account_table";
    if (!boost::filesystem::exists(data_path)) {
        std::cout << "database not found at " << data_path.string() << std::endl;
        return -1;
    }
    //std::cout << "load accounts ..." << std::endl;
    std::shared_ptr<shared_mutex> mutex = std::make_shared<shared_mutex>();
    database::account_database accounts(data_path, mutex);
    accounts.start();
    auto vec_accs = accounts.get_accounts();
    //std::cout << "loaded " << vec_accs->size() << " accounts." << std::endl;
    for (auto& vec : *vec_accs) {
        //if (vec.get_name() != argv[1] || vec.get_passwd() != passwd_hash)
        //    continue;
        std::cout << "Name: " << vec.get_name()<< std::endl;
        std::string mnemonic;
        std::cout << "Mnemonic: " << vec.get_mnemonic(std::string(argv[2]), mnemonic) << std::endl << std::endl;
    }

    return 0;
}

