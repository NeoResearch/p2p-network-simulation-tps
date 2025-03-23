 /*
    network.inject_transactions(500);
    network.print_peer_summary_matrix();
    
    network.inject_transactions(15);
    network.print_peer_summary_matrix();
    
    network.broadcast(100);
    network.print_peer_summary_matrix();
    
    network.prepare_request();
    network.print_publish_request_summary(PUBLISH_THRESHOLD);
    {
        int temp_simulated_time = 0;
        network.publish_proposed_transactions(PUBLISH_THRESHOLD, BLOCKTIME, temp_simulated_time, 100);
    }
    network.print_peer_summary_matrix();
    */