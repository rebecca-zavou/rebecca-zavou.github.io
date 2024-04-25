import React from "react";
import {Link} from 'react-router-dom'
import BannerImage from "../assets/prog.avif"
import "../styles/Home.css"

function Home() {
    return (
    <div className="home" style={{ backgroundImage: `url(${BannerImage})` }}>
        <div className="headerContainer">
            <h2>Department of Informatics and Telecommunications</h2>
            <h2> </h2>
            <p>National and Kapodistrian University of Athens</p>
            <Link to="/contact">
              <button>Contact Here!</button>
            </Link>
        </div>
    </div>
  );
}

export default Home;
